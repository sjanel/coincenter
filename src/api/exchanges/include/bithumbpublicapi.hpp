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

class BithumbPublic : public ExchangePublic {
 public:
  BithumbPublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _currenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _currenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override;

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFees(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get().find(currencyCode)->second;
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = 10) override { return _allOrderBooksCache.get(depth); }

  MarketOrderBook queryOrderBook(Market m, int depth = 10) override { return _orderbookCache.get(m, depth); }

  static constexpr char kUrlBase[] = "https://api.bithumb.com";
  static constexpr char kUserAgent[] = "Bithumb C++ API Client";

 private:
  friend class BithumbPrivate;

  struct CurrenciesFunc {
    CurrenciesFunc(CoincenterInfo& config, CurlHandle& curlHandle) : _config(config), _curlHandle(curlHandle) {}

    CurrencyExchangeFlatSet operator()();

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
  };

  struct WithdrawalFeesFunc {
    WithdrawalFeeMap operator()();

    CurlHandle _curlHandle;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _config(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBookMap operator()(int depth);

    CoincenterInfo& _config;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
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
  CachedResult<CurrenciesFunc> _currenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
};

}  // namespace api
}  // namespace cct