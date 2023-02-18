#pragma once

#include <limits>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;

class HuobiPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.huobi.pro", "https://api-aws.huobi.pro"};

  static constexpr char kUserAgent[] = "Huobi C++ API Client";

  static constexpr int kHuobiStandardOrderBookDefaultDepth = 150;

  HuobiPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kHuobiStandardOrderBookDefaultDepth) override {
    return _orderbookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  LastTradesVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override { return _tickerCache.get(mk); }

  VolAndPriNbDecimals queryVolAndPriNbDecimals(Market mk);

  MonetaryAmount sanitizePrice(Market mk, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market mk, CurrencyCode fromCurrencyCode, MonetaryAmount vol,
                                MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class HuobiPrivate;

  struct TradableCurrenciesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TradableCurrenciesFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}
#endif

    json operator()();

    CurlHandle& _curlHandle;
  };

  struct MarketsFunc {
#ifndef CCT_AGGR_INIT_CXX20
    MarketsFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

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
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AllOrderBooksFunc(CachedResult<MarketsFunc>& marketsCache, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _marketsCache(marketsCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

    MarketOrderBookMap operator()(int depth);

    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct OrderBookFunc {
#ifndef CCT_AGGR_INIT_CXX20
    OrderBookFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

    MarketOrderBook operator()(Market mk, int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradedVolumeFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TradedVolumeFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}
#endif

    MonetaryAmount operator()(Market mk);

    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TickerFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}
#endif

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

  const ExchangeInfo& _exchangeInfo;
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
