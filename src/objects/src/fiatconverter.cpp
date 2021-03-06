#include "fiatconverter.hpp"

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"

namespace cct {
namespace {

string LoadCurrencyConverterAPIKey(std::string_view dataDir) {
  static constexpr std::string_view kDefaultCommunityKey = "b25453de7984135a084b";
  // example http://free.currconv.com/api/v7/currencies?apiKey=b25453de7984135a084b
  static constexpr std::string_view kThirdPartySecretFileName = "thirdparty_secret.json";
  File thirdPartySecret(dataDir, File::Type::kSecret, kThirdPartySecretFileName, File::IfError::kNoThrow);
  json data = thirdPartySecret.readJson();
  if (data.empty() || data["freecurrencyconverter"].get<std::string_view>() == kDefaultCommunityKey) {
    log::warn("Unable to find custom Free Currency Converter key in {}", kThirdPartySecretFileName);
    log::warn("If you want to use extensively coincenter, please create your own key by going to");
    log::warn("https://free.currencyconverterapi.com/free-api-key and place it in");
    log::warn("'{}/secret/{}' like this:", dataDir, kThirdPartySecretFileName);
    log::warn(R"(  {"freecurrencyconverter": "<YourKey>"})");
    log::warn("Using default key provided as a demo to the community");
    return string(kDefaultCommunityKey);
  }
  return data["freecurrencyconverter"];
}

constexpr std::string_view kRatesCacheFile = "ratescache.json";

File GetRatesCacheFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kCache, kRatesCacheFile, File::IfError::kNoThrow);
}

constexpr std::string_view kFiatConverterBaseUrl = "https://free.currconv.com/api";
}  // namespace

FiatConverter::FiatConverter(const CoincenterInfo& coincenterInfo, Duration ratesUpdateFrequency)
    : _curlHandle(kFiatConverterBaseUrl, coincenterInfo.metricGatewayPtr(), Duration::zero(),
                  coincenterInfo.getRunMode()),
      _ratesUpdateFrequency(ratesUpdateFrequency),
      _apiKey(LoadCurrencyConverterAPIKey(coincenterInfo.dataDir())),
      _dataDir(coincenterInfo.dataDir()) {
  File ratesCacheFile = GetRatesCacheFile(_dataDir);
  json data = ratesCacheFile.readJson();
  _pricesMap.reserve(data.size());
  for (const auto& [marketStr, rateAndTimeData] : data.items()) {
    double rate = rateAndTimeData["rate"];
    int64_t timeepoch = rateAndTimeData["timeepoch"];
    log::trace("Stored rate {} for market {} from {}", rate, marketStr, kRatesCacheFile);
    _pricesMap.insert_or_assign(Market(marketStr, '-'),
                                PriceTimedValue{rate, TimePoint(std::chrono::seconds(timeepoch))});
  }
  log::debug("Loaded {} fiat currency rates from {}", _pricesMap.size(), kRatesCacheFile);
}

void FiatConverter::updateCacheFile() const {
  json data;
  for (const auto& [market, priceTimeValue] : _pricesMap) {
    string marketPairStr = market.assetsPairStrUpper('-');
    data[marketPairStr]["rate"] = priceTimeValue.rate;
    data[marketPairStr]["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(priceTimeValue.lastUpdatedTime.time_since_epoch()).count();
  }
  GetRatesCacheFile(_dataDir).write(data);
}

std::optional<double> FiatConverter::queryCurrencyRate(Market m) {
  string qStr(m.assetsPairStrUpper('_'));
  CurlOptions opts(HttpRequestType::kGet, {{"q", qStr}, {"apiKey", _apiKey}});

  string method = "/v7/convert?";
  method.append(opts.getPostData().str());
  opts.getPostData().clear();

  string dataStr = _curlHandle.query(method, std::move(opts));
  json data = json::parse(dataStr, nullptr, false /* allow exceptions */);
  //{"query":{"count":1},"results":{"EUR_KRW":{"id":"EUR_KRW","val":1329.475323,"to":"KRW","fr":"EUR"}}}
  if (data == json::value_t::discarded || !data.contains("results") || !data["results"].contains(qStr)) {
    log::error("No JSON data received from fiat currency converter service");
    auto it = _pricesMap.find(m);
    if (it != _pricesMap.end()) {
      // Update cache time anyway to avoid querying too much the service
      TimePoint t = Clock::now();
      it->second.lastUpdatedTime = t;
      _pricesMap[m.reverse()].lastUpdatedTime = t;
    }
    return std::nullopt;
  }
  const json& res = data["results"];
  const json& rates = res[qStr];
  double rate = rates["val"];
  log::debug("Stored rate {} for market {}", rate, qStr.c_str());
  TimePoint t = Clock::now();
  _pricesMap.insert_or_assign(m.reverse(), PriceTimedValue{static_cast<double>(1) / rate, t});
  _pricesMap.insert_or_assign(std::move(m), PriceTimedValue{rate, t});
  return rate;
}

double FiatConverter::convert(double amount, CurrencyCode from, CurrencyCode to) {
  if (from == to) {
    return amount;
  }
  Market m(from, to);
  double rate;
  std::lock_guard<std::mutex> guard(_pricesMutex);
  auto it = _pricesMap.find(m);
  if (it != _pricesMap.end() && Clock::now() - it->second.lastUpdatedTime < _ratesUpdateFrequency) {
    rate = it->second.rate;
  } else {
    if (_ratesUpdateFrequency == Duration::max()) {
      throw exception("Unable to query fiat currency rates and no rate found in cache");
    }
    std::optional<double> queriedRate = queryCurrencyRate(m);
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
