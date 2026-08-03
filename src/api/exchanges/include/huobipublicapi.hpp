#pragma once

#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "httpclient.hpp"
#include "currencycode.hpp"
#include "exchange-asset-config.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "huobi-schema.hpp"
#include "public-trade-vector.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class FiatConverter;

namespace api {
class CommonAPI;

class HuobiPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.huobi.pro", "https://api-aws.huobi.pro"};

  static constexpr int kHuobiStandardOrderBookDefaultDepth = 150;

  HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return queryTradableCurrencies().getOrThrow(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override;

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override;

  // Huobi lists withdrawable currencies whose fee is not a fixed amount (withdrawFeeType 'ratio' or
  // 'circulated', e.g. LUNC): those cannot be represented as a fixed MonetaryAmount and are therefore
  // absent from queryWithdrawalFees(). The source is thus not reliable for every withdrawable currency.
  bool isWithdrawalFeesSourceReliable() const override { return false; }

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

  MonetaryAmount sanitizeVolume(Market mk, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class HuobiPrivate;

  struct TradableCurrenciesFunc {
    schema::huobi::V2ReferenceCurrency operator()();

    HttpClient& _httpClient;
  };

  struct MarketsFunc {
    struct MarketInfo {
      VolAndPriNbDecimals volAndPriNbDecimals;

      MonetaryAmount minOrderValue;
      MonetaryAmount maxOrderValueUSDT;

      MonetaryAmount limitMinOrderAmount;
      MonetaryAmount limitMaxOrderAmount;

      MonetaryAmount sellMarketMinOrderAmount;
      MonetaryAmount sellMarketMaxOrderAmount;

      MonetaryAmount buyMarketMaxOrderValue;
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

  struct WithdrawParams {
    MonetaryAmount minWithdrawAmt;
    MonetaryAmount maxWithdrawAmt;
    int8_t withdrawPrecision = std::numeric_limits<int8_t>::max();
  };

  WithdrawParams getWithdrawParams(CurrencyCode cur);

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
