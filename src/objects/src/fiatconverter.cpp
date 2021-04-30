#include "fiatconverter.hpp"

#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace {
constexpr char kRatesFileName[] = ".ratescache";
constexpr char kCurrencyConverterBaseUrl[] = "https://free.currconv.com/api/v7";
constexpr char kAPIKey[] = "b25453de7984135a084b";
// example http://free.currconv.com/api/v7/currencies?apiKey=b25453de7984135a084b

}  // namespace

FiatConverter::FiatConverter(Clock::duration ratesUpdateFrequency, bool loadFromFileCacheAtInit)
    : _ratesUpdateFrequency(ratesUpdateFrequency) {
  if (loadFromFileCacheAtInit) {
    json data = OpenJsonFile(kRatesFileName, FileNotFoundMode::kNoThrow);
    _pricesMap.reserve(data.size());
    for (const auto& [marketStr, rateAndTimeData] : data.items()) {
      Market m(marketStr, '-');
      double rate = rateAndTimeData["rate"];
      int64_t timeepoch = rateAndTimeData["timeepoch"];
      log::debug("Stored rate {} for market {} from cache file", rate, marketStr);
      _pricesMap.insert_or_assign(m, PriceTimedValue{rate, TimePoint(std::chrono::seconds(timeepoch))});
    }
  }
}

void FiatConverter::updateCacheFile() const {
  json data;
  for (const auto& [market, priceTimeValue] : _pricesMap) {
    data[market.assetsPairStr('-')]["rate"] = priceTimeValue.rate;
    data[market.assetsPairStr('-')]["timeepoch"] =
        std::chrono::duration_cast<std::chrono::seconds>(priceTimeValue.lastUpdatedTime.time_since_epoch()).count();
  }
  WriteJsonFile(kRatesFileName, data);
}

double FiatConverter::queryCurrencyRate(Market m) {
  std::string url = kCurrencyConverterBaseUrl;
  url.append("/convert?");

  CurlOptions opts(CurlOptions::RequestType::kGet);
  std::string qStr(m.assetsPairStr('_'));
  opts.postdata.append("q", qStr);
  opts.postdata.append("apiKey", kAPIKey);

  url.append(opts.postdata.c_str());
  opts.postdata.clear();

  json data = json::parse(_curlHandle.query(url, opts));
  //{"query":{"count":1},"results":{"EUR_KRW":{"id":"EUR_KRW","val":1329.475323,"to":"KRW","fr":"EUR"}}}
  const json& res = data["results"];
  if (!res.contains(qStr)) {
    throw exception("Cannot find data for " + qStr);
  }
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
  auto it = _pricesMap.find(m);
  if (it == _pricesMap.end() || it->second.lastUpdatedTime + _ratesUpdateFrequency < Clock::now()) {
    rate = queryCurrencyRate(m);
  } else {
    rate = it->second.rate;
  }

  return amount * rate;
}

MonetaryAmount FiatConverter::convert(MonetaryAmount amount, CurrencyCode to) {
  return MonetaryAmount(convert(amount.toDouble(), amount.currencyCode(), to), to);
}

}  // namespace cct