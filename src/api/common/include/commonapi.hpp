#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string_view>

#include "binance-common-api.hpp"
#include "cache-file-updator-interface.hpp"
#include "cachedresult.hpp"
#include "cct_const.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencycodevector.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"
#include "withdrawalfees-crawler.hpp"

namespace cct {
class CoincenterInfo;
namespace api {
/// Public API connected to different exchanges, providing fast methods to retrieve huge amount of data.
class CommonAPI : public CacheFileUpdatorInterface {
 public:
  enum class AtInit : int8_t { kLoadFromFileCache, kNoLoadFromFileCache };

  explicit CommonAPI(const CoincenterInfo &coincenterInfo, Duration fiatsUpdateFrequency = std::chrono::days(4),
                     Duration withdrawalFeesUpdateFrequency = std::chrono::days(2),
                     AtInit atInit = AtInit::kLoadFromFileCache);

  /// Returns a new set of fiat currencies.
  CurrencyCodeSet queryFiats();

  /// Tells whether given currency code is a fiat currency or not.
  /// Fiat currencies are traditional currencies, such as EUR, USD, GBP, KRW, etc.
  /// Information here: https://en.wikipedia.org/wiki/Fiat_money
  bool queryIsCurrencyCodeFiat(CurrencyCode currencyCode);

  std::optional<MonetaryAmount> tryQueryWithdrawalFee(ExchangeNameEnum exchangeNameEnum, CurrencyCode currencyCode);

  /// Query withdrawal fees from crawler sources. It's not guaranteed to work though.
  MonetaryAmountByCurrencySet tryQueryWithdrawalFees(ExchangeNameEnum exchangeNameEnum);

  BinanceGlobalInfos &getBinanceGlobalInfos() { return _binanceGlobalInfos; }

  void updateCacheFile() const override;

 private:
  class FiatsFunc {
   public:
    explicit FiatsFunc(const CoincenterInfo &coincenterInfo);

    CurrencyCodeSet operator()();

    CurrencyCodeVector retrieveFiatsSource1();
    CurrencyCodeVector retrieveFiatsSource2();

   private:
    CurlHandle _curlHandle1;
    CurlHandle _curlHandle2;
  };

  CachedResultVault _cachedResultVault;
  const CoincenterInfo &_coincenterInfo;
  // A single mutex is needed as the cached result vault is shared
  std::recursive_mutex _globalMutex;
  CachedResult<FiatsFunc> _fiatsCache;
  BinanceGlobalInfos _binanceGlobalInfos;
  WithdrawalFeesCrawler _withdrawalFeesCrawler;
};
}  // namespace api
}  // namespace cct