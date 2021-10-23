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
class CoincenterInfo;
namespace api {
/// Public API connected to different exchanges, providing fast methods to retrieve huge amount of data.
class CryptowatchAPI : public ExchangeBase {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  explicit CryptowatchAPI(const CoincenterInfo &config, settings::RunMode runMode = settings::RunMode::kProd,
                          Clock::duration fiatsUpdateFrequency = std::chrono::hours(6),
                          bool loadFromFileCacheAtInit = true);

  CryptowatchAPI(const CryptowatchAPI &) = delete;
  CryptowatchAPI &operator=(const CryptowatchAPI &) = delete;

  CryptowatchAPI(CryptowatchAPI &&) noexcept = default;
  CryptowatchAPI &operator=(CryptowatchAPI &&) = delete;

  /// Tells whether given exchange is supported by Cryptowatch.
  bool queryIsExchangeSupported(const string &exchangeName) { return _supportedExchanges.get().contains(exchangeName); }

  /// Query the approximate price of market 'm' for exchange name 'exchangeName'.
  /// Data may not be up to date, but should respond quickly.
  std::optional<double> queryPrice(std::string_view exchangeName, Market m);

  /// Tells whether given currency code is a fiat currency or not.
  /// Fiat currencies are traditionnal currencies, such as EUR, USD, GBP, KRW, etc.
  /// Information here: https://en.wikipedia.org/wiki/Fiat_money
  bool queryIsCurrencyCodeFiat(CurrencyCode currencyCode) { return _fiatsCache.get().contains(currencyCode); }

  void updateCacheFile() const override;

 private:
  using Fiats = FlatSet<CurrencyCode>;
  using SupportedExchanges = FlatSet<string>;
  /// Cryptowatch markets are represented by one unique string pair, it's not trivial to split the two currencies
  /// acronyms. A second match will be needed to transform it to a final 'Market'
  using PricesPerMarketMap = std::unordered_map<string, double>;

  struct SupportedExchangesFunc {
    explicit SupportedExchangesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}

    SupportedExchanges operator()();

    CurlHandle &_curlHandle;
  };

  struct AllPricesFunc {
    explicit AllPricesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}

    json operator()();

    CurlHandle &_curlHandle;
  };

  struct FiatsFunc {
    explicit FiatsFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}

    Fiats operator()();

    CurlHandle &_curlHandle;
  };

  const CoincenterInfo &_config;
  CurlHandle _curlHandle;
  CachedResult<FiatsFunc> _fiatsCache;
  CachedResult<SupportedExchangesFunc> _supportedExchanges;
  CachedResult<AllPricesFunc> _allPricesCache;
};
}  // namespace api
}  // namespace cct