#pragma once

#include <mutex>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <utility>

#include "cache-file-updator-interface.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_flatset.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
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
class FiatConverter : public CacheFileUpdatorInterface {
 public:
  /// Creates a FiatConverter able to perform live queries to free converter api.
  /// @param ratesUpdateFrequency the minimum time needed between two currency rates updates
  FiatConverter(const CoincenterInfo &coincenterInfo, Duration ratesUpdateFrequency);

  /// Creates a FiatConverter able to perform live queries to free converter api.
  /// @param ratesUpdateFrequency the minimum time needed between two currency rates updates
  /// @param fiatsRatesCacheReader the reader from which to load the initial rates conversion cache
  /// @param thirdPartySecretReader the reader from which to load the third party secret
  FiatConverter(const CoincenterInfo &coincenterInfo, Duration ratesUpdateFrequency,
                const Reader &fiatsRatesCacheReader, const Reader &thirdPartySecretReader);

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
  void updateCacheFile() const override;

 private:
  struct PriceTimedValue {
    double rate;
    int64_t timeepoch;

    TimePoint lastUpdatedTime() const { return TimePoint(seconds(timeepoch)); }
  };

  struct ThirdPartySecret {
    string freecurrencyconverter;
  };

  static ThirdPartySecret LoadCurrencyConverterAPIKey(const Reader &thirdPartySecretReader);

  std::optional<double> queryCurrencyRate(Market market);

  std::optional<double> queryCurrencyRateSource1(Market market);
  std::optional<double> queryCurrencyRateSource2(Market market);

  enum class CacheReadMode : int8_t { kOnlyRecentRates, kUseAllRates };

  std::optional<double> retrieveRateFromCache(Market market, CacheReadMode cacheReadMode);

  void store(Market market, double rate);

  void refreshLastUpdatedTime(Market market);

  using PricesMap = std::unordered_map<Market, PriceTimedValue>;

  // For the algorithm computing rates
  struct Node {
    // hard limit to avoid unreasonable long paths and memory allocations
    static constexpr std::size_t kMaxCurrencyPathSize = 6U;

    using CurrencyPath = FixedCapacityVector<CurrencyCode, kMaxCurrencyPathSize>;

    using trivially_relocatable = std::true_type;

    CurrencyPath currencyPath;
    double rate;
    TimePoint oldestTs;
  };

  vector<Node> _nodes;
  using VisitedCurrencyCodesSet = FlatSet<CurrencyCode>;

  VisitedCurrencyCodesSet _visitedCurrencies;
  vector<std::pair<Market, PriceTimedValue>> _tmpPriceRatesVector;

  CurlHandle _curlHandle1;
  CurlHandle _curlHandle2;
  PricesMap _pricesMap;
  Duration _ratesUpdateFrequency;
  std::mutex _pricesMutex;
  ThirdPartySecret _thirdPartySecret;
  string _dataDir;
};
}  // namespace cct
