#include "cryptowatchapi.hpp"

#include <cctype>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_toupperlower.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace api {
namespace {
constexpr char kUrlBase[] = "https://api.cryptowat.ch";
constexpr char kUserAgent[] = "Cryptowatch C++ API Client";
constexpr char kFiatFileName[] = ".fiatcache";

std::string Query(CurlHandle& curlHandle, std::string_view method, CurlPostData&& postData = CurlPostData()) {
  std::string method_url = kUrlBase;
  method_url.push_back('/');
  method_url.append(method);

  CurlOptions opts(CurlOptions::RequestType::kGet);
  opts.userAgent = kUserAgent;
  opts.postdata = std::move(postData);

  return curlHandle.query(method_url, opts);
}

const json& CollectResults(const json& dataJson) {
  if (dataJson.contains("error") && !dataJson["error"].empty()) {
    throw exception("Cryptowatch::query error: " + std::string(dataJson["error"].front()));
  }
  return dataJson["result"];
}
}  // namespace

CryptowatchAPI::CryptowatchAPI(settings::RunMode runMode, Clock::duration fiatsUpdateFrequency,
                               bool loadFromFileCacheAtInit)
    : _curlHandle(Clock::duration::zero(), runMode),
      _fiatsUpdateFrequency(fiatsUpdateFrequency),
      _supportedExchanges(CachedResultOptions(std::chrono::hours(96), _cachedResultVault), _curlHandle),
      _allPricesCache(CachedResultOptions(std::chrono::seconds(10), _cachedResultVault), _curlHandle) {
  if (loadFromFileCacheAtInit) {
    json data = OpenJsonFile(kFiatFileName, FileNotFoundMode::kNoThrow);
    if (!data.empty()) {
      int64_t timeepoch = data["timeepoch"];
      _lastUpdatedFiatsTime = TimePoint(std::chrono::seconds(timeepoch));
      _fiats.reserve(static_cast<Fiats::size_type>(data["fiats"].size()));
      for (std::string fiatCode : data["fiats"]) {
        log::debug("Storing fiat {} from cache file", fiatCode);
        _fiats.emplace(std::move(fiatCode));
      }
      log::info("Stored {} fiats from cache file", _fiats.size());
    }
  }
}

std::optional<double> CryptowatchAPI::queryPrice(std::string_view exchangeName, Market m) {
  const PricesPerMarketMap& allPrices = _allPricesCache.get(exchangeName);
  auto it = allPrices.find(m.assetsPairStr());
  if (it == allPrices.end()) {
    it = allPrices.find(m.reverse().assetsPairStr());
    if (it == allPrices.end()) {
      return std::optional<double>();
    }
    return static_cast<double>(1) / it->second;
  }
  return it->second;
}

bool CryptowatchAPI::queryIsCurrencyCodeFiat(CurrencyCode currencyCode) {
  if (_fiats.empty() || _lastUpdatedFiatsTime + _fiatsUpdateFrequency < Clock::now()) {
    queryFiats();
  }
  return _fiats.contains(currencyCode);
}

void CryptowatchAPI::queryFiats() {
  json dataJson = json::parse(Query(_curlHandle, "assets"));
  const json& result = CollectResults(dataJson);
  _fiats.clear();
  for (const json& assetDetails : result) {
    if (assetDetails.contains("fiat") && assetDetails["fiat"]) {
      CurrencyCode fiatCode(assetDetails["symbol"].get<std::string_view>());
      _fiats.emplace(fiatCode);
      log::debug("Storing fiat {}", fiatCode.str());
    }
  }
  log::info("Stored {} fiats", _fiats.size());
  _lastUpdatedFiatsTime = Clock::now();
}

CryptowatchAPI::SupportedExchanges CryptowatchAPI::SupportedExchangesFunc ::operator()() {
  SupportedExchanges ret;
  json dataJson = json::parse(Query(_curlHandle, "exchanges"));
  const json& result = CollectResults(dataJson);
  for (const json& exchange : result) {
    std::string exchangeNameLowerCase = exchange["symbol"];
    log::debug("{} is supported by Cryptowatch", exchangeNameLowerCase);
    ret.insert(exchangeNameLowerCase);
  }
  log::info("{} exchanges supported by Cryptowatch", ret.size());
  return ret;
}

CryptowatchAPI::PricesPerMarketMap CryptowatchAPI::AllPricesFunc::operator()(std::string_view exchangeName) {
  json dataJson = json::parse(Query(_curlHandle, "markets/prices"));
  const json& result = CollectResults(dataJson);
  std::string marketPrefix = "market:" + std::string(exchangeName) + ":";
  size_t marketPrefixLen = marketPrefix.size();
  // {"result":{"market:kraken:ethdai":1493.844,"market:kraken:etheur":1238.14, ...},
  // "allowance":{"cost":0.015,"remaining":9.943,"upgrade":"For unlimited API access..."}}
  PricesPerMarketMap ret;
  for (const auto& [key, price] : result.items()) {
    if (key.starts_with(marketPrefix)) {
      std::string marketStr = key.substr(marketPrefixLen);
      std::transform(marketStr.begin(), marketStr.end(), marketStr.begin(), [](char c) { return cct::toupper(c); });
      ret.insert_or_assign(std::move(marketStr), static_cast<double>(price));
    }
  }
  log::debug("Retrieved {} prices from Cryptowatch all prices call", ret.size());
  return ret;
}

void CryptowatchAPI::updateCacheFile() const {
  json data = OpenJsonFile(kFiatFileName, FileNotFoundMode::kNoThrow);
  if (data.contains("timeepoch")) {
    int64_t lastTimeFileUpdated = data["timeepoch"];
    if (TimePoint(std::chrono::seconds(lastTimeFileUpdated)) > _lastUpdatedFiatsTime) {
      return;  // No update, file data is more up to date than our data
    }
  }
  data.clear();
  for (CurrencyCode fiatCode : _fiats) {
    data["fiats"].emplace_back(fiatCode.str());
  }
  data["timeepoch"] =
      std::chrono::duration_cast<std::chrono::seconds>(_lastUpdatedFiatsTime.time_since_epoch()).count();
  WriteJsonFile(kFiatFileName, data);
}
}  // namespace api
}  // namespace cct