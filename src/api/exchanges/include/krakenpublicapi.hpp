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

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get().find(currencyCode)->second;
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) override { return _allOrderBooksCache.get(depth); }

  MarketOrderBook queryOrderBook(Market m, int depth = 10) override { return _orderBookCache.get(m, depth); }

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
    explicit WithdrawalFeesFunc(const std::string& name) : _name(name) {}
    WithdrawalFeeMap operator()();
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

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderBookCache;
};
}  // namespace api
}  // namespace cct