#include "fiatconverter.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "permanentcurloptions.hpp"
#include "reader.hpp"
#include "timedef.hpp"

namespace cct {
namespace {

constexpr std::string_view kRatesCacheFile = "ratescache.json";

constexpr std::string_view kFiatConverterSource1BaseUrl = "https://free.currconv.com";
constexpr std::string_view kFiatConverterSource2BaseUrl = "https://api.vatcomply.com/rates";

string LoadCurrencyConverterAPIKey(std::string_view dataDir) {
  static constexpr std::string_view kDefaultCommunityKey = "b25453de7984135a084b";
  // example http://free.currconv.com/api/v7/currencies?apiKey=b25453de7984135a084b
  static constexpr std::string_view kThirdPartySecretFileName = "thirdparty_secret.json";

  const File thirdPartySecret(dataDir, File::Type::kSecret, kThirdPartySecretFileName, File::IfError::kNoThrow);
  json::container data = thirdPartySecret.readAllJson();
  auto freeConverterIt = data.find("freecurrencyconverter");
  if (freeConverterIt == data.end() || freeConverterIt->get<std::string_view>() == kDefaultCommunityKey) {
    log::warn("Unable to find custom Free Currency Converter key in {}", kThirdPartySecretFileName);
    log::warn("If you want to use extensively coincenter, please create your own key by going to");
    log::warn("https://free.currencyconverterapi.com/free-api-key and place it in");
    log::warn("'{}/secret/{}' like this:", dataDir, kThirdPartySecretFileName);
    log::warn(R"(  {"freecurrencyconverter": "<YourKey>"})");
    log::warn("Using default key provided as a demo to the community");
    return string(kDefaultCommunityKey);
  }
  return std::move(freeConverterIt->get_ref<string&>());
}

}  // namespace

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency)
    : FiatConverter(coincenterInfo, ratesUpdateFrequency, GetRatesCacheFile(coincenterInfo.dataDir())) {}

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency,
                             const Reader& fiatsCacheReader)
    : _curlHandle1(kFiatConverterSource1BaseUrl, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()),
      _curlHandle2(kFiatConverterSource2BaseUrl, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                   coincenterInfo.getRunMode()),
      _ratesUpdateFrequency(ratesUpdateFrequency),
      _apiKey(LoadCurrencyConverterAPIKey(coincenterInfo.dataDir())),
      _dataDir(coincenterInfo.dataDir()) {
  const json::container data = fiatsCacheReader.readAllJson();

  _pricesMap.reserve(data.size());
  for (const auto& [marketStr, rateAndTimeData] : data.items()) {
    const double rate = rateAndTimeData["rate"];
    const int64_t timeStamp = rateAndTimeData["timeepoch"];

    log::trace("Stored rate {} for market {} from {}", rate, marketStr, kRatesCacheFile);
    _pricesMap.insert_or_assign(Market(marketStr, '-'), PriceTimedValue{rate, TimePoint(seconds(timeStamp))});
  }
  log::debug("Loaded {} fiat currency rates from {}", _pricesMap.size(), kRatesCacheFile);
}

File FiatConverter::GetRatesCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, kRatesCacheFile, File::IfError::kNoThrow};
}

void FiatConverter::updateCacheFile() const {
  json::container data;
  for (const auto& [market, priceTimeValue] : _pricesMap) {
    const string marketPairStr = market.assetsPairStrUpper('-');

    data[marketPairStr]["rate"] = priceTimeValue.rate;
    data[marketPairStr]["timeepoch"] = TimestampToSecondsSinceEpoch(priceTimeValue.lastUpdatedTime);
  }
  GetRatesCacheFile(_dataDir).writeJson(data);
}

std::optional<double> FiatConverter::queryCurrencyRate(Market market) {
  auto ret = queryCurrencyRateSource1(market);
  if (ret) {
    return ret;
  }
  ret = queryCurrencyRateSource2(market);
  return ret;
}

std::optional<double> FiatConverter::queryCurrencyRateSource1(Market market) {
  const auto qStr = market.assetsPairStrUpper('_');

  const CurlOptions opts(HttpRequestType::kGet, {{"q", qStr}, {"apiKey", _apiKey}});

  const auto dataStr = _curlHandle1.query("/api/v7/convert", opts);

  static constexpr bool kAllowExceptions = false;
  const auto data = json::container::parse(dataStr, nullptr, kAllowExceptions);

  //{"query":{"count":1},"results":{"EUR_KRW":{"id":"EUR_KRW","val":1329.475323,"to":"KRW","fr":"EUR"}}}
  const auto resultsIt = data.find("results");
  if (data.is_discarded() || resultsIt == data.end() || !resultsIt->contains(qStr)) {
    log::warn("No JSON data received from fiat currency converter service's first source for pair '{}'", market);
    refreshLastUpdatedTime(market);
    return std::nullopt;
  }
  const auto& rates = (*resultsIt)[qStr];
  const double rate = rates["val"];
  store(market, rate);
  return rate;
}

std::optional<double> FiatConverter::queryCurrencyRateSource2(Market market) {
  const auto dataStr = _curlHandle2.query("", CurlOptions(HttpRequestType::kGet));
  static constexpr bool kAllowExceptions = false;
  const json::container jsonData = json::container::parse(dataStr, nullptr, kAllowExceptions);
  if (jsonData.is_discarded()) {
    log::error("Invalid response received from fiat currency converter service's second source");
    return {};
  }
  const auto baseIt = jsonData.find("base");
  const auto ratesIt = jsonData.find("rates");
  if (baseIt == jsonData.end() || ratesIt == jsonData.end()) {
    log::warn("No JSON data received from fiat currency converter service's second source", market);
    return {};
  }

  const TimePoint nowTime = Clock::now();

  _baseRateSource2 = baseIt->get<std::string_view>();
  for (const auto& [currencyCodeStr, rate] : ratesIt->items()) {
    const double rateDouble = rate.get<double>();
    const CurrencyCode currencyCode(currencyCodeStr);

    _pricesMap.insert_or_assign(Market(_baseRateSource2, currencyCode), PriceTimedValue(rateDouble, nowTime));
  }
  return retrieveRateFromCache(market);
}

void FiatConverter::store(Market market, double rate) {
  log::debug("Stored rate {} for {}", rate, market);
  const TimePoint nowTime = Clock::now();

  _pricesMap.insert_or_assign(market.reverse(), PriceTimedValue(static_cast<double>(1) / rate, nowTime));
  _pricesMap.insert_or_assign(std::move(market), PriceTimedValue(rate, nowTime));
}

void FiatConverter::refreshLastUpdatedTime(Market market) {
  const auto it = _pricesMap.find(market);
  if (it != _pricesMap.end()) {
    // Update cache time anyway to avoid querying too much the service
    const TimePoint nowTime = Clock::now();

    it->second.lastUpdatedTime = nowTime;
    _pricesMap[market.reverse()].lastUpdatedTime = nowTime;
  }
}

std::optional<double> FiatConverter::convert(double amount, CurrencyCode from, CurrencyCode to) {
  if (from == to) {
    return amount;
  }
  const Market market(from, to);

  double rate;

  std::lock_guard<std::mutex> guard(_pricesMutex);

  const auto optRate = retrieveRateFromCache(market);
  if (optRate) {
    rate = *optRate;
  } else {
    if (_ratesUpdateFrequency == Duration::max()) {
      log::error("Unable to query fiat currency rates and no rate found in cache for {}", market);
      return {};
    }
    std::optional<double> queriedRate = queryCurrencyRate(market);
    if (queriedRate) {
      rate = *queriedRate;
    } else {
      const auto it = _pricesMap.find(market);
      if (it == _pricesMap.end()) {
        log::error("Unable to query fiat currency rates and no rate found in cache for {}", market);
        return {};
      }
      log::warn("Fiat currency rate service unavailable, use not up to date currency rate in cache");
      rate = it->second.rate;
    }
  }

  return amount * rate;
}

std::optional<double> FiatConverter::retrieveRateFromCache(Market market) const {
  const auto rateIfYoung = [this, nowTime = Clock::now()](Market mk) -> std::optional<double> {
    const auto it = _pricesMap.find(mk);
    if (it != _pricesMap.end() && nowTime - it->second.lastUpdatedTime < _ratesUpdateFrequency) {
      return it->second.rate;
    }
    return {};
  };
  const auto directRate = rateIfYoung(market);
  if (directRate) {
    return directRate;
  }
  if (_baseRateSource2.isDefined()) {
    // Try with dual rates from base source.
    const auto rateBase1 = rateIfYoung(Market(_baseRateSource2, market.base()));
    if (rateBase1) {
      const auto rateBase2 = rateIfYoung(Market(_baseRateSource2, market.quote()));
      if (rateBase2) {
        return *rateBase2 / *rateBase1;
      }
    }
  }
  return {};
}
}  // namespace cct
