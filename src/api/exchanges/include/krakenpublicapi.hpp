#pragma once

#include <string>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;

namespace api {
class CryptowatchAPI;
class TradeOptions;

class KrakenPublic : public ExchangePublic {
 public:
  KrakenPublic(CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MonetaryAmount queryVolumeOrderMin(Market m) { return _marketsCache.get().second.find(m)->second.minVolumeOrder; }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get().first; }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get().first.find(currencyCode)->second;
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderBookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tickerCache.get(m).first; }

  MonetaryAmount queryLastPrice(Market m) override { return _tickerCache.get(m).second; }

  void updateCacheFile() const override;

  static constexpr char kUrlBase[] = "https://api.kraken.com";
  static constexpr char kVersion = '0';
  static constexpr char kUserAgent[] = "Kraken C++ API Client";

 private:
  friend class KrakenPrivate;

  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(CoincenterInfo& config, const ExchangeInfo& exchangeInfo, CurlHandle& curlHandle)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    CurrencyExchangeFlatSet operator()();

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct WithdrawalFeesFunc {
    using WithdrawalMinMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
    using WithdrawalInfoMaps = std::pair<WithdrawalFeeMap, WithdrawalMinMap>;

    WithdrawalFeesFunc(CoincenterInfo& config, CurlHandle& curlHandle, const std::string& name)
        : _config(config), _curlHandle(curlHandle), _name(name) {}

    WithdrawalInfoMaps operator()();

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const std::string& _name;
  };

  struct MarketsFunc {
    MarketsFunc(CoincenterInfo& config, CachedResult<TradableCurrenciesFunc>& currenciesCache, CurlHandle& curlHandle,
                const ExchangeInfo& exchangeInfo)
        : _tradableCurrenciesCache(currenciesCache),
          _config(config),
          _curlHandle(curlHandle),
          _exchangeInfo(exchangeInfo) {}

    struct MarketInfo {
      VolAndPriNbDecimals volAndPriNbDecimals;
      MonetaryAmount minVolumeOrder;
    };

    using MarketInfoMap = std::unordered_map<Market, MarketInfo>;

    std::pair<MarketSet, MarketInfoMap> operator()();

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CoincenterInfo& config, CachedResult<TradableCurrenciesFunc>& currenciesCache,
                      CachedResult<MarketsFunc>& marketsCache, CurlHandle& curlHandle)
        : _tradableCurrenciesCache(currenciesCache),
          _marketsCache(marketsCache),
          _config(config),
          _curlHandle(curlHandle) {}

    MarketOrderBookMap operator()(int depth);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
  };

  struct OrderBookFunc {
    OrderBookFunc(CoincenterInfo& config, CachedResult<TradableCurrenciesFunc>& currenciesCache,
                  CachedResult<MarketsFunc>& marketsCache, CurlHandle& curlHandle)
        : _tradableCurrenciesCache(currenciesCache),
          _marketsCache(marketsCache),
          _config(config),
          _curlHandle(curlHandle) {}

    MarketOrderBook operator()(Market m, int count);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    using Last24hTradedVolumeAndLatestPricePair = std::pair<MonetaryAmount, MonetaryAmount>;

    TickerFunc(CachedResult<TradableCurrenciesFunc>& tradableCurrenciesCache, CurlHandle& curlHandle)
        : _tradableCurrenciesCache(tradableCurrenciesCache), _curlHandle(curlHandle) {}

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