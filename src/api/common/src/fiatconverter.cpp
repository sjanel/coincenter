#include "fiatconverter.hpp"

#include <algorithm>
#include <iterator>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "fiats-converter-responses-schema.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "permanentcurloptions.hpp"
#include "read-json.hpp"
#include "reader.hpp"
#include "timedef.hpp"
#include "write-json.hpp"

namespace cct {
namespace {

constexpr std::string_view kRatesCacheFile = "ratescache.json";
constexpr std::string_view kThirdPartySecretFileName = "thirdparty_secret.json";

constexpr std::string_view kFiatConverterSource1BaseUrl = "https://free.currconv.com";
constexpr std::string_view kFiatConverterSource2BaseUrl = "https://api.vatcomply.com/rates";

File GetRatesCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, kRatesCacheFile, File::IfError::kNoThrow};
}

File GetThirdPartySecretFile(std::string_view dataDir) {
  return {dataDir, File::Type::kSecret, kThirdPartySecretFileName, File::IfError::kNoThrow};
}

}  // namespace

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency)
    : FiatConverter(coincenterInfo, ratesUpdateFrequency, GetRatesCacheFile(coincenterInfo.dataDir()),
                    GetThirdPartySecretFile(coincenterInfo.dataDir())) {}

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency,
                             const Reader& fiatsRatesCacheReader, const Reader& thirdPartySecretReader)
    : _curlHandle1(kFiatConverterSource1BaseUrl, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()),
      _curlHandle2(kFiatConverterSource2BaseUrl, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()),
      _ratesUpdateFrequency(ratesUpdateFrequency),
      _thirdPartySecret(LoadCurrencyConverterAPIKey(thirdPartySecretReader)),
      _dataDir(coincenterInfo.dataDir()) {
  const auto data = fiatsRatesCacheReader.readAll();

  ReadExactJsonOrThrow(data, _pricesMap);

  log::debug("Loaded {} fiat currency rates from {}", _pricesMap.size(), kRatesCacheFile);
}

void FiatConverter::updateCacheFile() const {
  auto dataStr = WriteJsonOrThrow(_pricesMap);
  GetRatesCacheFile(_dataDir).write(dataStr);
}

std::optional<double> FiatConverter::queryCurrencyRate(Market market) {
  std::optional<double> ret;
  if (!_thirdPartySecret.freecurrencyconverter.empty()) {
    ret = queryCurrencyRateSource1(market);
    if (ret) {
      return ret;
    }
  }
  ret = queryCurrencyRateSource2(market);
  return ret;
}

std::optional<double> FiatConverter::queryCurrencyRateSource1(Market market) {
  const auto qStr = market.assetsPairStrUpper('_');

  const CurlOptions opts(HttpRequestType::kGet, {{"q", qStr}, {"apiKey", _thirdPartySecret.freecurrencyconverter}});

  const auto dataStr = _curlHandle1.query("/api/v7/convert", opts);

  schema::FreeCurrencyConverterResponse response;

  //{"query":{"count":1},"results":{"EUR_KRW":{"id":"EUR_KRW","val":1329.475323,"to":"KRW","fr":"EUR"}}}
  auto ec = ReadPartialJson(dataStr, "fiat currency converter service's first source", response);

  if (ec) {
    return {};
  }

  const auto ratesIt = response.results.find(qStr);
  if (ratesIt == response.results.end()) {
    log::warn("No JSON data received from fiat currency converter service's first source for pair '{}'", market);
    refreshLastUpdatedTime(market);
    return std::nullopt;
  }
  const double rate = ratesIt->second.val;
  store(market, rate);
  return rate;
}

std::optional<double> FiatConverter::queryCurrencyRateSource2(Market market) {
  const auto dataStr = _curlHandle2.query("", CurlOptions(HttpRequestType::kGet));

  schema::FiatRatesSource2Response response;

  auto ec = ReadPartialJson(dataStr, "fiat currency converter service's second source", response);

  if (ec) {
    return {};
  }

  for (const auto& [currencyCode, rateDouble] : response.rates) {
    store(Market(response.base, currencyCode), rateDouble);
  }

  return retrieveRateFromCache(market, CacheReadMode::kUseAllRates);
}

void FiatConverter::store(Market market, double rate) {
  log::debug("Stored rate {} for {}", rate, market);
  const TimePoint nowTime = Clock::now();
  const auto ts = TimestampToSecondsSinceEpoch(nowTime);

  _pricesMap.insert_or_assign(std::move(market), PriceTimedValue(rate, ts));
}

void FiatConverter::refreshLastUpdatedTime(Market market) {
  const auto it = _pricesMap.find(market);
  if (it != _pricesMap.end()) {
    // Update cache time anyway to avoid querying too much the service
    const TimePoint nowTime = Clock::now();
    const auto ts = TimestampToSecondsSinceEpoch(nowTime);

    it->second.timeepoch = ts;
  }
}

std::optional<double> FiatConverter::convert(double amount, CurrencyCode from, CurrencyCode to) {
  if (from == to) {
    return amount;
  }

  const Market market(from, to);

  std::lock_guard<std::mutex> guard(_pricesMutex);

  // First query in the cache with not up to date rates
  auto optRate = retrieveRateFromCache(market, CacheReadMode::kOnlyRecentRates);
  if (optRate) {
    return amount * *optRate;
  }

  if (_ratesUpdateFrequency == Duration::max()) {
    log::error("Fiat converter live queries disabled and no rate found in cache for {}", market);
    return {};
  }

  // Updates the rates
  optRate = queryCurrencyRate(market);
  if (optRate) {
    return amount * *optRate;
  }

  // Query the rates from the update cache
  optRate = retrieveRateFromCache(market, CacheReadMode::kUseAllRates);
  if (optRate) {
    return amount * *optRate;
  }

  log::error("Unable to retrieve rate for {}", market);
  return {};
}

std::optional<double> FiatConverter::retrieveRateFromCache(Market market, CacheReadMode cacheReadMode) {
  // single rate check first
  auto nowTime = Clock::now();

  auto isPriceUpToDate = [this, nowTime](const auto& pair) {
    return nowTime - pair.second.lastUpdatedTime() < _ratesUpdateFrequency;
  };

  auto directConversionIt = _pricesMap.find(market);
  if (directConversionIt != _pricesMap.end() &&
      (cacheReadMode == CacheReadMode::kUseAllRates || isPriceUpToDate(*directConversionIt))) {
    return directConversionIt->second.rate;
  }

  if (cacheReadMode == CacheReadMode::kOnlyRecentRates) {
    _tmpPriceRatesVector.clear();
    std::ranges::copy_if(_pricesMap, std::back_inserter(_tmpPriceRatesVector), isPriceUpToDate);
  } else {
    _tmpPriceRatesVector.resize(_pricesMap.size());
    std::ranges::copy(_pricesMap, _tmpPriceRatesVector.begin());
  }

  struct NodeCompare {
    bool operator()(const Node& lhs, const Node& rhs) const {
      // We will use a heap with the smallest currency path first to favor the rate paths with the least number of
      // conversions
      return rhs.currencyPath.size() < lhs.currencyPath.size();
    }
  };

  NodeCompare comp;

  _nodes.resize(1, Node{Node::CurrencyPath(1U, market.base()), 1.0, nowTime});
  _visitedCurrencies.clear();

  while (!_nodes.empty()) {
    std::ranges::pop_heap(_nodes, comp);
    Node node = std::move(_nodes.back());
    _nodes.pop_back();

    auto cur = node.currencyPath.back();

    // stop criteria
    if (cur == market.quote()) {
      _pricesMap.insert_or_assign(market, PriceTimedValue(node.rate, TimestampToSecondsSinceEpoch(node.oldestTs)));
      return node.rate;
    }

    if (node.currencyPath.size() == node.currencyPath.max_size()) {
      log::warn("[fiat conversion] currency path too long for {}, stopping exploration", market);
      continue;
    }

    // Cache the visited currency to avoid exploration of same paths
    if (_visitedCurrencies.contains(cur)) {
      continue;
    }
    _visitedCurrencies.insert(cur);

    // generation of new nodes
    for (const auto& [mk, priceTimedValue] : _tmpPriceRatesVector) {
      if (cur == mk.base() && std::ranges::find(node.currencyPath, mk.quote()) == node.currencyPath.end()) {
        auto curPath = node.currencyPath;
        curPath.emplace_back(mk.quote());
        _nodes.emplace_back(std::move(curPath), node.rate * priceTimedValue.rate,
                            std::min(node.oldestTs, priceTimedValue.lastUpdatedTime()));
        std::ranges::push_heap(_nodes, comp);
      } else if (cur == mk.quote() && std::ranges::find(node.currencyPath, mk.base()) == node.currencyPath.end()) {
        auto curPath = node.currencyPath;
        curPath.emplace_back(mk.base());
        _nodes.emplace_back(std::move(curPath), node.rate / priceTimedValue.rate,
                            std::min(node.oldestTs, priceTimedValue.lastUpdatedTime()));
        std::ranges::push_heap(_nodes, comp);
      }
    }
  }

  return {};
}

FiatConverter::ThirdPartySecret FiatConverter::LoadCurrencyConverterAPIKey(const Reader& thirdPartySecretReader) {
  auto dataStr = thirdPartySecretReader.readAll();
  ThirdPartySecret thirdPartySecret;

  if (dataStr.empty()) {
    log::debug("No third party secret file found in {}", kThirdPartySecretFileName);
    return thirdPartySecret;
  }

  auto ec = ReadPartialJson(dataStr, "third party's secrets", thirdPartySecret);

  if (ec) {
    return thirdPartySecret;
  }

  if (thirdPartySecret.freecurrencyconverter.empty()) {
    log::debug("Unable to find custom Free Currency Converter key in {}", kThirdPartySecretFileName);
  }

  return thirdPartySecret;
}

}  // namespace cct
