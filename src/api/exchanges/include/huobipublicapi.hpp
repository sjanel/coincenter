#pragma once

#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
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

class HuobiPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.huobi.pro", "https://api-aws.huobi.pro"};

  static constexpr int kHuobiStandardOrderBookDefaultDepth = 150;

  HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CommonAPI& commonAPI);

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
    json operator()();

    CurlHandle& _curlHandle;
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

  struct WithdrawParams {
    MonetaryAmount minWithdrawAmt;
    MonetaryAmount maxWithdrawAmt;
    int8_t withdrawPrecision = std::numeric_limits<int8_t>::max();
  };

  WithdrawParams getWithdrawParams(CurrencyCode cur);

  static bool ShouldDiscardChain(CurrencyCode cur, const json& chainDetail);

  const ExchangeConfig& _exchangeConfig;
  CurlHandle _curlHandle;
  CurlHandle _healthCheckCurlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct
