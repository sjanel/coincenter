#pragma once

#include <string_view>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "currencycodeset.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CommonAPI;

class UpbitPublic : public ExchangePublic {
 public:
  UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeesSet queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) override {
    return _orderbookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  LastTradesVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override { return _tickerCache.get(mk); }

  static constexpr std::string_view kUrlBase = "https://api.upbit.com";

 private:
  friend class UpbitPrivate;

  static string ReverseMarketStr(Market mk) { return mk.reverse().assetsPairStrUpper('-'); }

  static bool CheckCurrencyCode(CurrencyCode standardCode, const CurrencyCodeSet& excludedCurrencies);

  static MonetaryAmount SanitizeVolume(MonetaryAmount vol, MonetaryAmount pri);

  struct MarketsFunc {
    MarketSet operator()();

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradableCurrenciesFunc {
    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct WithdrawalFeesFunc {
    WithdrawalFeesSet operator()();

    const string& _name;
    std::string_view _dataDir;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradedVolumeFunc {
    MonetaryAmount operator()(Market mk);

    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    MonetaryAmount operator()(Market mk);

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