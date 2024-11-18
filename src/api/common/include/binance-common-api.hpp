#pragma once

#include <mutex>

#include "binance-common-schema.hpp"
#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "runmodes.hpp"

namespace cct {

class AbstractMetricGateway;
class PermanentCurlOptions;

namespace api {

class BinanceGlobalInfos {
 public:
  BinanceGlobalInfos(CachedResultOptions&& cachedResultOptions, AbstractMetricGateway* pMetricGateway,
                     const PermanentCurlOptions& permanentCurlOptions, settings::RunMode runMode);

  MonetaryAmountByCurrencySet queryWithdrawalFees();

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode);

  CurrencyExchangeFlatSet queryTradableCurrencies(const CurrencyCodeSet& excludedCurrencies);

 private:
  friend class BinancePrivate;

  class BinanceGlobalInfosFunc {
   public:
    BinanceGlobalInfosFunc(AbstractMetricGateway* pMetricGateway, const PermanentCurlOptions& permanentCurlOptions,
                           settings::RunMode runMode);

    schema::binance::NetworkCoinDataVector operator()();

   private:
    CurlHandle _curlHandle;
  };

  static CurrencyExchangeFlatSet ExtractTradableCurrencies(
      const schema::binance::NetworkCoinDataVector& networkCoinDataVector, const CurrencyCodeSet& excludedCurrencies);

  std::mutex _mutex;
  CachedResult<BinanceGlobalInfosFunc> _globalInfosCache;
};

}  // namespace api
}  // namespace cct