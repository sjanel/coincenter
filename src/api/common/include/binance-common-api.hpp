#pragma once

#include <mutex>

#include "cachedresult.hpp"
#include "cct_json-container.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"

namespace cct {

class AbstractMetricGateway;
class PermanentCurlOptions;

namespace api {

class BinanceGlobalInfosFunc {
 public:
  BinanceGlobalInfosFunc(AbstractMetricGateway* pMetricGateway, const PermanentCurlOptions& permanentCurlOptions,
                         settings::RunMode runMode);

  json::container operator()();

 private:
  CurlHandle _curlHandle;
};

class BinanceGlobalInfos {
 public:
  BinanceGlobalInfos(CachedResultOptions&& cachedResultOptions, AbstractMetricGateway* pMetricGateway,
                     const PermanentCurlOptions& permanentCurlOptions, settings::RunMode runMode);

  MonetaryAmountByCurrencySet queryWithdrawalFees();

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode);

  CurrencyExchangeFlatSet queryTradableCurrencies(const CurrencyCodeSet& excludedCurrencies);

 private:
  friend class BinancePrivate;

  static CurrencyExchangeFlatSet ExtractTradableCurrencies(const json::container& allCoins,
                                                           const CurrencyCodeSet& excludedCurrencies);

  std::mutex _mutex;
  CachedResult<BinanceGlobalInfosFunc> _globalInfosCache;
};

}  // namespace api
}  // namespace cct