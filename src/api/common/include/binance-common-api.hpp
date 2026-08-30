#pragma once

#include <mutex>

#include "binance-common-schema.hpp"
#include "cachedresult.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "httpclient.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "runmodes.hpp"

namespace cct {

class AbstractMetricGateway;
class PermanentRequestOptions;

namespace api {

class BinanceGlobalInfos {
 public:
  BinanceGlobalInfos(CachedResultOptions&& cachedResultOptions, AbstractMetricGateway* pMetricGateway,
                     const PermanentRequestOptions& permanentHttpRequestOptions, settings::RunMode runMode);

  MonetaryAmountByCurrencySet queryWithdrawalFees();

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode);

  CurrencyExchangeFlatSet queryTradableCurrencies(const CurrencyCodeSet& excludedCurrencies);

 private:
  friend class BinancePrivate;

  class BinanceGlobalInfosFunc {
   public:
    BinanceGlobalInfosFunc(AbstractMetricGateway* pMetricGateway,
                           const PermanentRequestOptions& permanentHttpRequestOptions, settings::RunMode runMode);

    schema::binance::NetworkCoinDataVector operator()();

   private:
    HttpClient _httpClient;
  };

  static CurrencyExchangeFlatSet ExtractTradableCurrencies(
      const schema::binance::NetworkCoinDataVector& networkCoinDataVector, const CurrencyCodeSet& excludedCurrencies);

  std::mutex _mutex;
  CachedResult<BinanceGlobalInfosFunc> _globalInfosCache;
};

}  // namespace api
}  // namespace cct