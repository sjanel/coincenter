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
    TradableCurrenciesFunc(CurlHandle& curlHandle, const CoincenterInfo& coincenterInfo, CryptowatchAPI& cryptowatchApi)
        : _curlHandle(curlHandle), _coincenterInfo(coincenterInfo), _cryptowatchApi(cryptowatchApi) {}

    struct CurrencyInfo {
      explicit CurrencyInfo(CurrencyCode c) : currencyExchange(c, c, c) {}
      explicit CurrencyInfo(CurrencyExchange&& c) : currencyExchange(std::move(c)) {}

      auto operator<=>(const CurrencyInfo& o) const = default;

      CurrencyExchange currencyExchange;
      MonetaryAmount withdrawalMinSize;
      MonetaryAmount withdrawalMinFee;
    };

    using CurrencyInfoSet = FlatSet<CurrencyInfo>;

    CurrencyInfoSet operator()();

    CurlHandle& _curlHandle;
    const CoincenterInfo& _coincenterInfo;
    CryptowatchAPI& _cryptowatchApi;
  };

  struct MarketsFunc {
    MarketsFunc(CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

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
    AllOrderBooksFunc(CachedResult<MarketsFunc>& marketsCache, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _marketsCache(marketsCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketOrderBookMap operator()(int depth);

    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
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
