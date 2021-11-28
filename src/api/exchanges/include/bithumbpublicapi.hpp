#pragma once

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"
#include "timehelpers.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;

class BithumbPublic : public ExchangePublic {
 public:
  BithumbPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override;

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderbookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tradedVolumeCache.get(m); }

  LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market m) override;

  static constexpr std::string_view kUrlBase = "https://api.bithumb.com";
  static constexpr char kUserAgent[] = "Bithumb C++ API Client";

 private:
  friend class BithumbPrivate;

  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(const CoincenterInfo& config, CryptowatchAPI& cryptowatchAPI, CurlHandle& curlHandle)
        : _coincenterInfo(config), _cryptowatchAPI(cryptowatchAPI), _curlHandle(curlHandle) {}

    CurrencyExchangeFlatSet operator()();

    const CoincenterInfo& _coincenterInfo;
    CryptowatchAPI& _cryptowatchAPI;
    CurlHandle& _curlHandle;
  };

  struct WithdrawalFeesFunc {
    WithdrawalFeesFunc(AbstractMetricGateway* pMetricGateway, Clock::duration minDurationBetweenQueries,
                       settings::RunMode runMode)
        : _curlHandle(pMetricGateway, minDurationBetweenQueries, runMode) {}

    WithdrawalFeeMap operator()();

    CurlHandle _curlHandle;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(const CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _coincenterInfo(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBookMap operator()(int depth);

    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct OrderBookFunc {
    OrderBookFunc(const CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _coincenterInfo(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBook operator()(Market m, int depth);

    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradedVolumeFunc {
    explicit TradedVolumeFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}

    MonetaryAmount operator()(Market m);

    CurlHandle& _curlHandle;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
};

}  // namespace api
}  // namespace cct