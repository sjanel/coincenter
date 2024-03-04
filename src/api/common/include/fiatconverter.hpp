#pragma once

#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "file.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "reader.hpp"
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
/// Fallback mechanism exists if api key does not exist or is expired.
///
/// Conversion methods are thread safe.
class FiatConverter {
 public:
  static File GetRatesCacheFile(std::string_view dataDir);

  /// Creates a FiatConverter able to perform live queries to free converter api.
  /// @param ratesUpdateFrequency the minimum time needed between two currency rates updates
  FiatConverter(const CoincenterInfo &coincenterInfo, Duration ratesUpdateFrequency);

  /// Creates a FiatConverter able to perform live queries to free converter api.
  /// @param ratesUpdateFrequency the minimum time needed between two currency rates updates
  /// @param reader the reader from which to load the initial rates conversion cache
  FiatConverter(const CoincenterInfo &coincenterInfo, Duration ratesUpdateFrequency, const Reader &reader);

  std::optional<double> convert(double amount, CurrencyCode from, CurrencyCode to);

  std::optional<MonetaryAmount> convert(MonetaryAmount amount, CurrencyCode to) {
    auto optDouble = convert(amount.toDouble(), amount.currencyCode(), to);
    if (optDouble) {
      return MonetaryAmount(*optDouble, to);
    }
    return {};
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

  std::optional<double> queryCurrencyRateSource1(Market mk);
  std::optional<double> queryCurrencyRateSource2(Market mk);

  std::optional<double> retrieveRateFromCache(Market mk) const;

  void store(Market mk, double rate);

  void refreshLastUpdatedTime(Market mk);

  using PricesMap = std::unordered_map<Market, PriceTimedValue>;

  CurlHandle _curlHandle1;
  CurlHandle _curlHandle2;
  PricesMap _pricesMap;
  Duration _ratesUpdateFrequency;
  std::mutex _pricesMutex;
  string _apiKey;
  string _dataDir;
  CurrencyCode _baseRateSource2;
};
}  // namespace cct
