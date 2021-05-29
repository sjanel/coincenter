#include "fiatconverter.hpp"

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace {
constexpr char kRatesFileName[] = ".ratescache.json";
constexpr char kCurrencyConverterBaseUrl[] = "https://free.currconv.com/api/v7";
constexpr char kFiatConverterJsonKeyFile[] = "thirdparty_secret.json";

std::string LoadCurrencyConverterAPIKey() {
  constexpr char kDefaultCommunityKey[] = "b25453de7984135a084b";
  // example http://free.currconv.com/api/v7/currencies?apiKey=b25453de7984135a084b

  json data = OpenJsonFile(kFiatConverterJsonKeyFile, FileNotFoundMode::kNoThrow, FileType::kConfig);
  if (data.empty() || data["freecurrencyconverter"].get<std::string_view>() == kDefaultCommunityKey) {
    log::warn("Unable to find custom Free Currency Converter key in {}", kFiatConverterJsonKeyFile);
    log::warn("If you want to use extensively coincenter, please create your own key by going to");
    log::warn("https://free.currencyconverterapi.com/free-api-key and place it in");
    log::warn("'config/thirdparty_secret.json' like this:");
    log::warn("  {\"freecurrencyconverter\": \"<YourKey>\"}");
    log::warn("Using default key provided as a demo to the community");
    return kDefaultCommunityKey;
  }
  return std::string(data["freecurrencyconverter"].get<std::string_view>());
}

}  // namespace

FiatConverter::FiatConverter(Clock::duration ratesUpdateFrequency, bool loadFromFileCacheAtInit)
    : _ratesUpdateFrequency(ratesUpdateFrequency), _apiKey(LoadCurrencyConverterAPIKey()) {
  if (loadFromFileCacheAtInit) {
    json data = OpenJsonFile(kRatesFileName, FileNotFoundMode::kNoThrow, FileType::kData);
    _pricesMap.reserve(data.size());
    for (const auto& [marketStr, rateAndTimeData] : data.items()) {
      Market m(marketStr, '-');
      double rate = rateAndTimeData["rate"];
      int64_t timeepoch = rateAndTimeData["timeepoch"];
      log::trace("Stored rate {} for market {} from {}", rate, marketStr, kRatesFileName);
      _pricesMap.insert_or_assign(m, PriceTimedValue{rate, TimePoint(std::chrono::seconds(timeepoch))});
    }
    log::debug("Loaded {} fiat currency rates from {}", _pricesMap.size(), kRatesFileName);
  }
}

void FiatConverter::updateCacheFile() const {
  json data;
  for (const auto& [market, priceTimeValue] : _pricesMap) {
    std::string marketPairStr = market.assetsPairStr('-');
    data[marketPairStr]["rate"] = priceTimeValue.rate;
    data[marketPairStr]["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(priceTimeValue.lastUpdatedTime.time_since_epoch()).count();
  }
  WriteJsonFile(kRatesFileName, data, FileType::kData);
}

std::optional<double> FiatConverter::queryCurrencyRate(Market m) {
  std::string url = kCurrencyConverterBaseUrl;
  url.append("/convert?");

  CurlOptions opts(CurlOptions::RequestType::kGet);
  std::string qStr(m.assetsPairStr('_'));
  opts.postdata.append("q", qStr);
  opts.postdata.append("apiKey", _apiKey);

  url.append(opts.postdata.c_str());
  opts.postdata.clear();

  json data = json::parse(_curlHandle.query(url, opts), nullptr, false /* allow exceptions */);
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
  log::debug("Stored rate {} for market {}", rate, qStr);
  TimePoint t = Clock::now();
  _pricesMap.insert_or_assign(m, PriceTimedValue{rate, t});
  _pricesMap.insert_or_assign(m.reverse(), PriceTimedValue{static_cast<double>(1) / rate, t});
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
    if (_ratesUpdateFrequency == Clock::duration::max()) {
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
