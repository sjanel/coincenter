#pragma once

#include <limits>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_flatset.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;

class KucoinPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kUrlBase = "https://api.kucoin.com";

  static constexpr char kUserAgent[] = "Kucoin C++ API Client";

  static constexpr int kKucoinStandardOrderBookDefaultDepth = 20;

  KucoinPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = 10) override { return _orderbookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tradedVolumeCache.get(m); }

  LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market m) override { return _tickerCache.get(m); }

  VolAndPriNbDecimals queryVolAndPriNbDecimals(Market m);

  MonetaryAmount sanitizePrice(Market m, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market m, MonetaryAmount vol);

 private:
  friend class KucoinPrivate;

  struct TradableCurrenciesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    TradableCurrenciesFunc(CurlHandle& curlHandle, const CoincenterInfo& coincenterInfo, CryptowatchAPI& cryptowatchApi)
        : _curlHandle(curlHandle), _coincenterInfo(coincenterInfo), _cryptowatchApi(cryptowatchApi) {}
#endif

    struct CurrencyInfo {
#ifndef CCT_AGGR_INIT_CXX20
      explicit CurrencyInfo(CurrencyCode c) : currencyExchange(c) {}

      CurrencyInfo(CurrencyExchange&& c, MonetaryAmount wMS, MonetaryAmount wMF)
          : currencyExchange(std::move(c)), withdrawalMinSize(wMS), withdrawalMinFee(wMF) {}
#endif

      auto operator<=>(const CurrencyInfo& o) const { return currencyExchange <=> o.currencyExchange; }

      CurrencyExchange currencyExchange;
      MonetaryAmount withdrawalMinSize{};
      MonetaryAmount withdrawalMinFee{};
    };

    using CurrencyInfoSet = FlatSet<CurrencyInfo>;

    CurrencyInfoSet operator()();

    CurlHandle& _curlHandle;
    const CoincenterInfo& _coincenterInfo;
    CryptowatchAPI& _cryptowatchApi;
  };

  struct MarketsFunc {
#ifndef CCT_AGGR_INIT_CXX20
    MarketsFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

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

    MarketOrderBook operator()(Market m, int depth);

    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct TradedVolumeFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TradedVolumeFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}
#endif

    MonetaryAmount operator()(Market m);

    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TickerFunc(CurlHandle& curlHandle) : _curlHandle(curlHandle) {}
#endif

    MonetaryAmount operator()(Market m);

    CurlHandle& _curlHandle;
  };

  static CurlPostData GetSymbolPostData(Market m) { return CurlPostData{{"symbol", m.assetsPairStrUpper('-')}}; }

  const ExchangeInfo& _exchangeInfo;
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
