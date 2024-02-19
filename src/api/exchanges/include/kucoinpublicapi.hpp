#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "curlhandle.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeConfig;
class FiatConverter;

namespace api {
class CommonAPI;

class KucoinPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kUrlBase = "https://api.kucoin.com";

  static constexpr int kKucoinStandardOrderBookDefaultDepth = 20;

  KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override;

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = 10) override { return _orderbookCache.get(mk, depth); }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  TradesVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override { return _tickerCache.get(mk); }

  VolAndPriNbDecimals queryVolAndPriNbDecimals(Market mk);

  MonetaryAmount sanitizePrice(Market mk, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market mk, MonetaryAmount vol);

 private:
  friend class KucoinPrivate;

  struct TradableCurrenciesFunc {
    struct CurrencyInfo {
      auto operator<=>(const CurrencyInfo& o) const { return currencyExchange <=> o.currencyExchange; }

      CurrencyExchange currencyExchange;
      MonetaryAmount withdrawalMinSize{};
      MonetaryAmount withdrawalMinFee{};
    };

    using CurrencyInfoSet = FlatSet<CurrencyInfo>;

    CurrencyInfoSet operator()();

    CurlHandle& _curlHandle;
    const CoincenterInfo& _coincenterInfo;
    CommonAPI& _commonApi;
  };

  struct MarketsFunc {
    struct MarketInfo {
      MonetaryAmount baseMinSize;
      MonetaryAmount quoteMinSize;  // quote is synonym of price
      MonetaryAmount baseMaxSize;
      MonetaryAmount quoteMaxSize;
      MonetaryAmount baseIncrement;
      MonetaryAmount priceIncrement;

      CurrencyCode feeCurrency;
    };

    using MarketInfoMap = std::unordered_map<Market, MarketInfo>;

    std::pair<MarketSet, MarketInfoMap> operator()();

    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int depth);

    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct TradedVolumeFunc {
    MonetaryAmount operator()(Market mk);

    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    MonetaryAmount operator()(Market mk);

    CurlHandle& _curlHandle;
  };

  static CurlPostData GetSymbolPostData(Market mk) { return CurlPostData{{"symbol", mk.assetsPairStrUpper('-')}}; }

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct
