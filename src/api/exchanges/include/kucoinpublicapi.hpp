#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "httpclient.hpp"
#include "httppostdata.hpp"
#include "currencycode.hpp"
#include "exchange-asset-config.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "public-trade-vector.hpp"
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

  static constexpr std::string_view kStatusCodeOK = "200000";

  static constexpr int kKucoinStandardOrderBookDefaultDepth = 20;

  KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return queryTradableCurrencies().getOrThrow(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override;

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) override {
    return _orderbookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  PublicTradeVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

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

    HttpClient& _httpClient;
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

    HttpClient& _httpClient;
    const schema::ExchangeAssetConfig& _assetConfig;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    CachedResult<MarketsFunc>& _marketsCache;
    HttpClient& _httpClient;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int depth);

    HttpClient& _httpClient;
  };

  struct TradedVolumeFunc {
    MonetaryAmount operator()(Market mk);

    HttpClient& _httpClient;
  };

  struct TickerFunc {
    MonetaryAmount operator()(Market mk);

    HttpClient& _httpClient;
  };

  static HttpPostData GetSymbolPostData(Market mk) { return HttpPostData{{"symbol", mk.assetsPairStrUpper('-')}}; }

  HttpClient _httpClient;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct
