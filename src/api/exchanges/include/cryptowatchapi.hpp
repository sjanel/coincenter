#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "cct_run_modes.hpp"
#include "curlhandle.hpp"
#include "exchangebase.hpp"
#include "market.hpp"

namespace cct {
namespace api {
/// Public API connected to different exchanges, providing fast methods to retrieve huge amount of data.
class CryptowatchAPI : public ExchangeBase {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  /// Cryptowatch markets are represented by one unique string pair, it's not trivial to split the two currencies
  /// acronyms. A second match will be needed to transform it to a final 'cct::Market'
  using PricesPerMarketMap = std::unordered_map<std::string, double>;

  explicit CryptowatchAPI(settings::RunMode runMode = settings::kProd,
                          Clock::duration fiatsUpdateFrequency = std::chrono::hours(6),
                          bool loadFromFileCacheAtInit = true);

  /// Tells whether given exchange is supported by Cryptowatch.
  bool queryIsExchangeSupported(const std::string &exchangeName) {
    return _supportedExchanges.get().contains(exchangeName);
  }

  /// Get a map containing all the average prices for all markets of given exchange.
  /// The Markets are represented as a unique string with the concatenation of both currency acronyms in upper case:
  /// Example: Market Bitcoin - Euro would have "BTCEUR" as key.
  PricesPerMarketMap queryAllPrices(std::string_view exchangeName) { return _allPricesCache.get(exchangeName); }

  /// Query the approximate price of market 'm' for exchange name 'exchangeName'.
  /// Data may not be up to date, but should respond quickly.
  std::optional<double> queryPrice(std::string_view exchangeName, Market m);

  /// Tells whether given currency code is a fiat currency or not.
  /// Fiat currencies are traditionnal currencies, such as EUR, USD, GBP, KRW, etc.
  /// Information here: https://en.wikipedia.org/wiki/Fiat_money
  bool queryIsCurrencyCodeFiat(CurrencyCode currencyCode);

  void updateCacheFile() const override;

 private:
  using Fiats = cct::FlatSet<CurrencyCode>;
  using SupportedExchanges = cct::FlatSet<std::string>;

  CryptowatchAPI(const CryptowatchAPI &) = delete;
  CryptowatchAPI(CryptowatchAPI &&) = default;
  CryptowatchAPI &operator=(const CryptowatchAPI &) = delete;
  CryptowatchAPI &operator=(CryptowatchAPI &&) = default;

  struct SupportedExchangesFunc {
    explicit SupportedExchangesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}
    SupportedExchanges operator()();
    CurlHandle &_curlHandle;
  };

  struct AllPricesFunc {
    explicit AllPricesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}

    PricesPerMarketMap operator()(std::string_view exchangeName);

    CurlHandle &_curlHandle;
  };

  void queryFiats();

  CurlHandle _curlHandle;
  Fiats _fiats;
  TimePoint _lastUpdatedFiatsTime;
  Clock::duration _fiatsUpdateFrequency;
  CachedResult<SupportedExchangesFunc> _supportedExchanges;
  CachedResult<AllPricesFunc, std::string> _allPricesCache;
};
}  // namespace api
}  // namespace cct