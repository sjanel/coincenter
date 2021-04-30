#pragma once

#include <chrono>
#include <unordered_map>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {

/// Service querying fiat currency exchange rates from a free API.
///
/// Current chosen source is, for now:
/// https://free.currconv.com/api/v7
///
/// More information here:
/// https://stackoverflow.com/questions/3139879/how-do-i-get-currency-exchange-rates-via-an-api-such-as-google-finance

class FiatConverter {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  explicit FiatConverter(Clock::duration ratesUpdateFrequency = std::chrono::hours(2),
                         bool loadFromFileCacheAtInit = true);

  FiatConverter(const FiatConverter &) = delete;
  FiatConverter &operator=(const FiatConverter &) = delete;
  FiatConverter(FiatConverter &&) = default;
  FiatConverter &operator=(FiatConverter &&) = default;

  double convert(double amount, CurrencyCode from, CurrencyCode to);

  MonetaryAmount convert(MonetaryAmount amount, CurrencyCode to);

  void updateCacheFile() const;

 private:
  struct PriceTimedValue {
    double rate;
    TimePoint lastUpdatedTime;
  };

  double queryCurrencyRate(Market m);

  using PricesMap = std::unordered_map<Market, PriceTimedValue>;

  CurlHandle _curlHandle;
  PricesMap _pricesMap;
  Clock::duration _ratesUpdateFrequency;
};
}  // namespace cct