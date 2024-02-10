#pragma once

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string_view>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "exchangebase.hpp"
#include "timedef.hpp"
#include "withdrawalfees-crawler.hpp"

namespace cct {
class CoincenterInfo;
namespace api {
/// Public API connected to different exchanges, providing fast methods to retrieve huge amount of data.
class CommonAPI : public ExchangeBase {
 public:
  using Fiats = FlatSet<CurrencyCode>;

  enum class AtInit : int8_t { kLoadFromFileCache, kNoLoadFromFileCache };

  explicit CommonAPI(const CoincenterInfo &coincenterInfo, Duration fiatsUpdateFrequency = std::chrono::hours(96),
                     Duration withdrawalFeesUpdateFrequency = std::chrono::hours(96),
                     AtInit atInit = AtInit::kLoadFromFileCache);

  /// Returns a new set of fiat currencies.
  Fiats queryFiats();

  /// Tells whether given currency code is a fiat currency or not.
  /// Fiat currencies are traditional currencies, such as EUR, USD, GBP, KRW, etc.
  /// Information here: https://en.wikipedia.org/wiki/Fiat_money
  bool queryIsCurrencyCodeFiat(CurrencyCode currencyCode);

  /// Query withdrawal fees from crawler sources. It's not guaranteed to work though.
  WithdrawalFeesCrawler::WithdrawalInfoMaps queryWithdrawalFees(std::string_view exchangeName);

  void updateCacheFile() const override;

 private:
  class FiatsFunc {
   public:
    FiatsFunc();

    Fiats operator()();

    vector<CurrencyCode> retrieveFiatsSource1();
    vector<CurrencyCode> retrieveFiatsSource2();

   private:
    CurlHandle _curlHandle1;
    CurlHandle _curlHandle2;
  };

  CachedResultVault _cachedResultVault;
  const CoincenterInfo &_coincenterInfo;
  // A single mutex is needed as the cached result vault is shared
  std::mutex _globalMutex;
  CachedResult<FiatsFunc> _fiatsCache;
  WithdrawalFeesCrawler _withdrawalFeesCrawler;
};
}  // namespace api
}  // namespace cct