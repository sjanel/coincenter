#include "fiatconverter.hpp"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "curloptions.hpp"
#include "currencycode.hpp"
#include "file.hpp"
#include "httprequesttype.hpp"
#include "market.hpp"
#include "permanentcurloptions.hpp"
#include "timedef.hpp"

namespace cct {
namespace {

string LoadCurrencyConverterAPIKey(std::string_view dataDir) {
  static constexpr std::string_view kDefaultCommunityKey = "b25453de7984135a084b";
  // example http://free.currconv.com/api/v7/currencies?apiKey=b25453de7984135a084b
  static constexpr std::string_view kThirdPartySecretFileName = "thirdparty_secret.json";
  File thirdPartySecret(dataDir, File::Type::kSecret, kThirdPartySecretFileName, File::IfError::kNoThrow);
  json data = thirdPartySecret.readAllJson();
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

constexpr std::string_view kRatesCacheFile = "ratescache.json";

File GetRatesCacheFile(std::string_view dataDir) {
  return {dataDir, File::Type::kCache, kRatesCacheFile, File::IfError::kNoThrow};
}

constexpr std::string_view kFiatConverterBaseUrl = "https://free.currconv.com";
}  // namespace

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency)
    : _curlHandle(kFiatConverterBaseUrl, coincenterInfo.metricGatewayPtr(), PermanentCurlOptions(),
                  coincenterInfo.getRunMode()),
      _ratesUpdateFrequency(ratesUpdateFrequency),
      _apiKey(LoadCurrencyConverterAPIKey(coincenterInfo.dataDir())),
      _dataDir(coincenterInfo.dataDir()) {
  File ratesCacheFile = GetRatesCacheFile(_dataDir);
  json data = ratesCacheFile.readAllJson();
  _pricesMap.reserve(data.size());
  for (const auto& [marketStr, rateAndTimeData] : data.items()) {
    double rate = rateAndTimeData["rate"];
    int64_t timeepoch = rateAndTimeData["timeepoch"];
    log::trace("Stored rate {} for market {} from {}", rate, marketStr, kRatesCacheFile);
    _pricesMap.insert_or_assign(Market(marketStr, '-'), PriceTimedValue{rate, TimePoint(TimeInS(timeepoch))});
  }
  log::debug("Loaded {} fiat currency rates from {}", _pricesMap.size(), kRatesCacheFile);
}

void FiatConverter::updateCacheFile() const {
  json data;
  for (const auto& [market, priceTimeValue] : _pricesMap) {
    string marketPairStr = market.assetsPairStrUpper('-');
    data[marketPairStr]["rate"] = priceTimeValue.rate;
    data[marketPairStr]["timeepoch"] = TimestampToS(priceTimeValue.lastUpdatedTime);
  }
  GetRatesCacheFile(_dataDir).write(data);
}

std::optional<double> FiatConverter::queryCurrencyRate(Market mk) {
  string qStr(mk.assetsPairStrUpper('_'));
  CurlOptions opts(HttpRequestType::kGet, {{"q", qStr}, {"apiKey", _apiKey}});
  std::string_view dataStr = _curlHandle.query("/api/v7/convert", opts);
  static constexpr bool kAllowExceptions = false;
  json data = json::parse(dataStr, nullptr, kAllowExceptions);
  //{"query":{"count":1},"results":{"EUR_KRW":{"id":"EUR_KRW","val":1329.475323,"to":"KRW","fr":"EUR"}}}
  if (data == json::value_t::discarded || !data.contains("results") || !data["results"].contains(qStr)) {
    log::error("No JSON data received from fiat currency converter service for pair '{}'", mk);
    auto it = _pricesMap.find(mk);
    if (it != _pricesMap.end()) {
      // Update cache time anyway to avoid querying too much the service
      TimePoint nowTime = Clock::now();
      it->second.lastUpdatedTime = nowTime;
      _pricesMap[mk.reverse()].lastUpdatedTime = nowTime;
    }
    return std::nullopt;
  }
  const json& res = data["results"];
  const json& rates = res[qStr];
  double rate = rates["val"];
  log::debug("Stored rate {} for market {}", rate, qStr);
  TimePoint nowTime = Clock::now();
  _pricesMap.insert_or_assign(mk.reverse(), PriceTimedValue{static_cast<double>(1) / rate, nowTime});
  _pricesMap.insert_or_assign(std::move(mk), PriceTimedValue{rate, nowTime});
  return rate;
}

double FiatConverter::convert(double amount, CurrencyCode from, CurrencyCode to) {
  if (from == to) {
    return amount;
  }
  Market mk(from, to);
  double rate;
  std::lock_guard<std::mutex> guard(_pricesMutex);
  auto it = _pricesMap.find(mk);
  if (it != _pricesMap.end() && Clock::now() - it->second.lastUpdatedTime < _ratesUpdateFrequency) {
    rate = it->second.rate;
  } else {
    if (_ratesUpdateFrequency == Duration::max()) {
      throw exception("Unable to query fiat currency rates and no rate found in cache");
    }
    std::optional<double> queriedRate = queryCurrencyRate(mk);
    if (queriedRate) {
      rate = *queriedRate;
    } else {
      if (it == _pricesMap.end()) {
        throw exception("Unable to query fiat currency rates and no rate found in cache");
      }
      log::warn("Fiat currency rate service unavailable, use not up to date currency rate in cache");
      rate = it->second.rate;
    }
  }

  return amount * rate;
}

}  // namespace cct
