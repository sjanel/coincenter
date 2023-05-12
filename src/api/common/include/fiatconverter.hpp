#pragma once

#include <mutex>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {

class CoincenterInfo;

/// Service querying fiat currency exchange rates from a free API.
///
/// Current chosen source is, for now:
/// https://free.currconv.com/api/v7
///
/// It requires an API key even for free usage.
///
/// Hard-coded key exists in case you don't have one but if you want to use extensively coincenter, please create your
/// own key on https://free.currencyconverterapi.com/free-api-key and place it in 'config/thirdparty_secret.json' file
/// such that 'coincenter' uses it instead of the hardcoded one. The reason is that API services are hourly limited and
/// reaching the limit would make it basically unusable for the community.
///
/// Conversion methods are thread safe.
class FiatConverter {
 public:
  /// Creates a FiatConverter able to perform live queries to free converter api.
  /// @param ratesUpdateFrequency the minimum time needed between two currency rates updates
  FiatConverter(const CoincenterInfo &coincenterInfo, Duration ratesUpdateFrequency);

  double convert(double amount, CurrencyCode from, CurrencyCode to);

  MonetaryAmount convert(MonetaryAmount amount, CurrencyCode to) {
    return MonetaryAmount(convert(amount.toDouble(), amount.currencyCode(), to), to);
  }

  /// Store rates in a file to make data persistent.
  /// This method is not thread-safe and is expected to be called only once before end of normal termination of program.
  void updateCacheFile() const;

 private:
  struct PriceTimedValue {
    double rate;
    TimePoint lastUpdatedTime;
  };

  std::optional<double> queryCurrencyRate(Market mk);

  using PricesMap = std::unordered_map<Market, PriceTimedValue>;

  CurlHandle _curlHandle;
  PricesMap _pricesMap;
  Duration _ratesUpdateFrequency;
  std::mutex _pricesMutex;
  string _apiKey;
  string _dataDir;
};
}  // namespace cct
