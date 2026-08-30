#pragma once

#include <string_view>
#include <unordered_map>
#include <utility>

#include "cache-file-updator-interface.hpp"
#include "cachedresult.hpp"
#include "cachedresultvault.hpp"
#include "currencycode.hpp"
#include "exchange-name-enum.hpp"
#include "httpclient.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "timedef.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class WithdrawalFeesCrawlerTest;
}

/// Retrieves public withdrawal fee schedules for exchanges whose regular public APIs do not expose them.
/// This class is non thread-safe.
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
  friend class api::WithdrawalFeesCrawlerTest;

  static WithdrawalInfoMaps ParseBithumbResponse(std::string_view dataStr);
  static WithdrawalInfoMaps ParseKrakenResponse(std::string_view dataStr);

  class WithdrawalFeesFunc {
   public:
    explicit WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo);

    WithdrawalInfoMaps operator()(ExchangeNameEnum exchangeNameEnum);

   private:
    HttpClient _httpClient;
  };

  const CoincenterInfo& _coincenterInfo;
  CachedResult<WithdrawalFeesFunc, ExchangeNameEnum> _withdrawalFeesCache;
};

}  // namespace cct
