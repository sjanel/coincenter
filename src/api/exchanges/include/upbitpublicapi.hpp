#pragma once

#include <string_view>

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
  UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderbookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tradedVolumeCache.get(m); }

  LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market m) override { return _tickerCache.get(m); }

  static constexpr std::string_view kUrlBase = "https://api.upbit.com";
  static constexpr char kUserAgent[] = "Upbit C++ API Client";

 private:
  friend class UpbitPrivate;

  static bool CheckCurrencyCode(CurrencyCode standardCode, const ExchangeInfo::CurrencySet& excludedCurrencies);

  struct MarketsFunc {
    MarketsFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketSet operator()();

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(CurlHandle& curlHandle, CachedResult<MarketsFunc>& marketsCache)
        : _curlHandle(curlHandle), _marketsCache(marketsCache) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct WithdrawalFeesFunc {
    WithdrawalFeesFunc(const string& name, std::string_view dataDir) : _name(name), _dataDir(dataDir) {}

    WithdrawalFeeMap operator()();

    const string& _name;
    string _dataDir;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo, CachedResult<MarketsFunc>& marketsCache)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo), _marketsCache(marketsCache) {}

    MarketOrderBookMap operator()(int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct OrderBookFunc {
    OrderBookFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBook operator()(Market m, int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradedVolumeFunc {
    explicit TradedVolumeFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}

    MonetaryAmount operator()(Market m);

    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    explicit TickerFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}

    MonetaryAmount operator()(Market m);

    CurlHandle& _curlHandle;
  };

  CurlHandle _curlHandle;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct