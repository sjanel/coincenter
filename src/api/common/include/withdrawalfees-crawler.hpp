#pragma once

#include <string_view>
#include <unordered_map>
#include <utility>

#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "coincenterinfo.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"

namespace cct {

/// This class is able to crawl some public withdrawal fees web pages in order to retrieve them from unofficial sources,
/// which is better than nothing. This class is non thread-safe.
class WithdrawalFeesCrawler {
 public:
  WithdrawalFeesCrawler(const CoincenterInfo& coincenterInfo, Duration minDurationBetweenQueries,
                        CachedResultVault& cachedResultVault);

  using WithdrawalMinMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
  using WithdrawalInfoMaps = std::pair<MonetaryAmountByCurrencySet, WithdrawalMinMap>;

  WithdrawalInfoMaps get(std::string_view exchangeName) { return _withdrawalFeesCache.get(exchangeName); }

  void updateCacheFile() const;

 private:
  class WithdrawalFeesFunc {
   public:
    explicit WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo);

    WithdrawalInfoMaps operator()(std::string_view exchangeName);

   private:
    WithdrawalInfoMaps get1(std::string_view exchangeName);
    WithdrawalInfoMaps get2(std::string_view exchangeName);

    CurlHandle _curlHandle1;
    CurlHandle _curlHandle2;
  };

  const CoincenterInfo& _coincenterInfo;
  CachedResult<WithdrawalFeesFunc, std::string_view> _withdrawalFeesCache;
};

}  // namespace cct