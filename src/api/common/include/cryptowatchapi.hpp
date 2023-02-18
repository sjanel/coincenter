#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangebase.hpp"
#include "market.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {
class CoincenterInfo;
namespace api {
/// Public API connected to different exchanges, providing fast methods to retrieve huge amount of data.
class CryptowatchAPI : public ExchangeBase {
 public:
  using Fiats = FlatSet<CurrencyCode>;

  explicit CryptowatchAPI(const CoincenterInfo &config, settings::RunMode runMode = settings::RunMode::kProd,
                          Duration fiatsUpdateFrequency = std::chrono::hours(96), bool loadFromFileCacheAtInit = true);

  /// Tells whether given exchange is supported by Cryptowatch.
  bool queryIsExchangeSupported(std::string_view exchangeName) {
    std::lock_guard<std::mutex> guard(_exchangesMutex);
    return _supportedExchanges.get().contains(exchangeName);
  }

  /// Query the approximate price of market 'mk' for exchange name 'exchangeName'.
  /// Data may not be up to date, but should respond quickly.
  std::optional<double> queryPrice(std::string_view exchangeName, Market mk);

  /// Returns a new set of fiat currencies.
  Fiats queryFiats() {
    std::lock_guard<std::mutex> guard(_fiatsMutex);
    return _fiatsCache.get();
  }

  /// Tells whether given currency code is a fiat currency or not.
  /// Fiat currencies are traditionnal currencies, such as EUR, USD, GBP, KRW, etc.
  /// Information here: https://en.wikipedia.org/wiki/Fiat_money
  bool queryIsCurrencyCodeFiat(CurrencyCode currencyCode) {
    std::lock_guard<std::mutex> guard(_fiatsMutex);
    return _fiatsCache.get().contains(currencyCode);
  }

  void updateCacheFile() const override;

 private:
  using SupportedExchanges = FlatSet<string, std::less<>>;
  /// Cryptowatch markets are represented by one unique string pair, it's not trivial to split the two currencies
  /// acronyms. A second match will be needed to transform it to a final 'Market'
  using PricesPerMarketMap = std::unordered_map<string, double>;

  struct SupportedExchangesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit SupportedExchangesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}
#endif

    SupportedExchanges operator()();

    CurlHandle &_curlHandle;
  };

  struct AllPricesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit AllPricesFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}
#endif

    json operator()();

    CurlHandle &_curlHandle;
  };

  struct FiatsFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit FiatsFunc(CurlHandle &curlHandle) : _curlHandle(curlHandle) {}
#endif

    Fiats operator()();

    CurlHandle &_curlHandle;
  };

  CachedResultVault _cachedResultVault;
  const CoincenterInfo &_coincenterInfo;
  CurlHandle _curlHandle;
  std::mutex _pricesMutex, _fiatsMutex, _exchangesMutex;
  CachedResult<FiatsFunc> _fiatsCache;
  CachedResult<SupportedExchangesFunc> _supportedExchanges;
  CachedResult<AllPricesFunc> _allPricesCache;
};
}  // namespace api
}  // namespace cct