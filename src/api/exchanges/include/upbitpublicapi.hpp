#pragma once

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;

class UpbitPublic : public ExchangePublic {
 public:
  UpbitPublic(CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _currenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _currenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFees(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get().find(currencyCode)->second;
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) override { return _allOrderBooksCache.get(depth); }

  MarketOrderBook queryOrderBook(Market m, int depth = 10) override { return _orderbookCache.get(m, depth); }

  static constexpr char kUrlBase[] = "https://api.upbit.com";
  static constexpr char kUserAgent[] = "Upbit C++ API Client";

 private:
  friend class UpbitPrivate;

  struct MarketsFunc {
    MarketsFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketSet operator()();

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct CurrenciesFunc {
    CurrenciesFunc(CurlHandle& curlHandle, CachedResult<MarketsFunc>& marketsCache)
        : _curlHandle(curlHandle), _marketsCache(marketsCache) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct WithdrawalFeesFunc {
    explicit WithdrawalFeesFunc(const std::string& name) : _name(name) {}
    WithdrawalFeeMap operator()();
    const std::string& _name;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo,
                      CachedResult<MarketsFunc>& marketsCache)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo), _marketsCache(marketsCache) {}

    MarketOrderBookMap operator()(int depth);

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct OrderBookFunc {
    OrderBookFunc(CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBook operator()(Market m, int depth);

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  CurlHandle _curlHandle;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<CurrenciesFunc> _currenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
};

}  // namespace api
}  // namespace cct