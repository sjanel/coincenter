#pragma once

#include "cachedresult.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"
#include "timehelpers.hpp"
#include "volumeandpricenbdecimals.hpp"
namespace cct {

class CoincenterInfo;
class ExchangeInfo;

namespace api {
class CryptowatchAPI;

class KrakenPublic : public ExchangePublic {
 public:
  KrakenPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MonetaryAmount queryVolumeOrderMin(Market m) { return _marketsCache.get().second.find(m)->second.minVolumeOrder; }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get().first; }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return false; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderBookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tickerCache.get(m).first; }

  LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market m) override { return _tickerCache.get(m).second; }

  void updateCacheFile() const override;

  static constexpr std::string_view kUrlBase = "https://api.kraken.com/0";
  static constexpr char kUserAgent[] = "Kraken C++ API Client";

 private:
  friend class KrakenPrivate;

  struct TradableCurrenciesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    TradableCurrenciesFunc(const CoincenterInfo& config, CryptowatchAPI& cryptowatchApi, CurlHandle& curlHandle,
                           const ExchangeInfo& exchangeInfo)
        : _coincenterInfo(config),
          _cryptowatchApi(cryptowatchApi),
          _curlHandle(curlHandle),
          _exchangeInfo(exchangeInfo) {}
#endif

    CurrencyExchangeFlatSet operator()();

    const CoincenterInfo& _coincenterInfo;
    CryptowatchAPI& _cryptowatchApi;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  class WithdrawalFeesFunc {
   public:
    using WithdrawalMinMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
    using WithdrawalInfoMaps = std::pair<WithdrawalFeeMap, WithdrawalMinMap>;

    WithdrawalFeesFunc(const CoincenterInfo& coincenterInfo, Clock::duration minDurationBetweenQueries);

    WithdrawalInfoMaps operator()();

   private:
    WithdrawalInfoMaps updateFromSource1();
    WithdrawalInfoMaps updateFromSource2();

    // Use different curl handles as it is not from Kraken official REST API.
    // There are two of them such that second one may be used in case of failure of the first one
    CurlHandle _curlHandle1, _curlHandle2;
  };

  struct MarketsFunc {
#ifndef CCT_AGGR_INIT_CXX20
    MarketsFunc(CachedResult<TradableCurrenciesFunc>& currenciesCache, const CoincenterInfo& config,
                CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _tradableCurrenciesCache(currenciesCache),
          _coincenterInfo(config),
          _curlHandle(curlHandle),
          _exchangeInfo(exchangeInfo) {}
#endif

    struct MarketInfo {
      VolAndPriNbDecimals volAndPriNbDecimals;
      MonetaryAmount minVolumeOrder;
    };

    using MarketInfoMap = std::unordered_map<Market, MarketInfo>;

    std::pair<MarketSet, MarketInfoMap> operator()();

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AllOrderBooksFunc(CachedResult<TradableCurrenciesFunc>& currenciesCache, CachedResult<MarketsFunc>& marketsCache,
                      const CoincenterInfo& config, CurlHandle& curlHandle)
        : _tradableCurrenciesCache(currenciesCache),
          _marketsCache(marketsCache),
          _coincenterInfo(config),
          _curlHandle(curlHandle) {}
#endif

    MarketOrderBookMap operator()(int depth);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
  };

  struct OrderBookFunc {
#ifndef CCT_AGGR_INIT_CXX20
    OrderBookFunc(CachedResult<TradableCurrenciesFunc>& currenciesCache, CachedResult<MarketsFunc>& marketsCache,
                  CurlHandle& curlHandle)
        : _tradableCurrenciesCache(currenciesCache), _marketsCache(marketsCache), _curlHandle(curlHandle) {}
#endif

    MarketOrderBook operator()(Market m, int count);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    using Last24hTradedVolumeAndLatestPricePair = std::pair<MonetaryAmount, MonetaryAmount>;

#ifndef CCT_AGGR_INIT_CXX20
    TickerFunc(CachedResult<TradableCurrenciesFunc>& tradableCurrenciesCache, CurlHandle& curlHandle)
        : _tradableCurrenciesCache(tradableCurrenciesCache), _curlHandle(curlHandle) {}
#endif

    Last24hTradedVolumeAndLatestPricePair operator()(Market m);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CurlHandle& _curlHandle;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderBookCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};
}  // namespace api
}  // namespace cct
