#pragma once

#include <string_view>
#include <unordered_map>
#include <utility>

#include "cache-file-updator-interface.hpp"
#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "cct_const.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"

namespace cct {

/// This class is able to crawl some public withdrawal fees web pages in order to retrieve them from unofficial sources,
/// which is better than nothing. This class is non thread-safe.
class WithdrawalFeesCrawler : public CacheFileUpdatorInterface {
 public:
  WithdrawalFeesCrawler(const CoincenterInfo& coincenterInfo, Duration minDurationBetweenQueries,
                        CachedResultVault& cachedResultVault);

  using WithdrawalMinMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
  using WithdrawalInfoMaps = std::pair<MonetaryAmountByCurrencySet, WithdrawalMinMap>;

  const WithdrawalInfoMaps& get(ExchangeNameEnum exchangeNameEnum) {
    return _withdrawalFeesCache.get(exchangeNameEnum);
  }

  void updateCacheFile() const override;

 private:
  class WithdrawalFeesFunc {
   public:
    explicit WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo);

    WithdrawalInfoMaps operator()(ExchangeNameEnum exchangeNameEnum);

   private:
    WithdrawalInfoMaps get1(ExchangeNameEnum exchangeNameEnum);
    WithdrawalInfoMaps get2(ExchangeNameEnum exchangeNameEnum);

    CurlHandle _curlHandle1;
    CurlHandle _curlHandle2;
  };

  const CoincenterInfo& _coincenterInfo;
  CachedResult<WithdrawalFeesFunc, ExchangeNameEnum> _withdrawalFeesCache;
};

}  // namespace cct