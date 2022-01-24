#pragma once

#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "timehelpers.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CryptowatchAPI;

class BinancePublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.binance.com", "https://api1.binance.com",
                                                   "https://api2.binance.com", "https://api3.binance.com"};

  static constexpr char kUserAgent[] = "Binance C++ API Client";

  BinancePublic(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter,
                api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override {
    return queryTradableCurrencies(_globalInfosCache.get());
  }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderbookCache.get(m, depth); }

  MonetaryAmount queryLast24hVolume(Market m) override { return _tradedVolumeCache.get(m); }

  LastTradesVector queryLastTrades(Market m, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market m) override { return _tickerCache.get(m); }

  MonetaryAmount sanitizePrice(Market m, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market m, MonetaryAmount vol, MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class BinancePrivate;

  CurrencyExchangeFlatSet queryTradableCurrencies(const json& data) const;

  class CommonInfo {
   public:
    static constexpr auto kNbBaseURLs = std::distance(std::begin(kURLBases), std::end(kURLBases));

    CommonInfo(const CoincenterInfo& coincenterInfo, const ExchangeInfo& exchangeInfo, settings::RunMode runMode);

    /// Get the Binance base URL providing the lowest response time thanks to periodic pings.
    std::string_view getBestBaseURL() { return _baseURLUpdater.get(); }

    const ExchangeInfo& _exchangeInfo;
    CurlHandle _curlHandle;

   private:
    struct BaseURLUpdater {
      std::string_view operator()();

      CurlHandle _curlHandles[kNbBaseURLs];
    };
    CachedResult<BaseURLUpdater> _baseURLUpdater;
  };

  struct ExchangeInfoFunc {
    using ExchangeInfoDataByMarket = std::unordered_map<Market, json>;

#ifndef CCT_AGGR_INIT_CXX20
    explicit ExchangeInfoFunc(CommonInfo& commonInfo) : _commonInfo(commonInfo) {}
#endif

    ExchangeInfoDataByMarket operator()();

    CommonInfo& _commonInfo;
  };

  struct GlobalInfosFunc {
    GlobalInfosFunc(AbstractMetricGateway* pMetricGateway, Duration minDurationBetweenQueries,
                    settings::RunMode runMode)
        : _curlHandle(pMetricGateway, minDurationBetweenQueries, runMode) {}

    json operator()();

    CurlHandle _curlHandle;
  };

  struct MarketsFunc {
#ifndef CCT_AGGR_INIT_CXX20
    MarketsFunc(CachedResult<ExchangeInfoFunc>& exchangeInfoCache, CurlHandle& curlHandle,
                const ExchangeInfo& exchangeInfo)
        : _exchangeInfoCache(exchangeInfoCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

    MarketSet operator()();

    CachedResult<ExchangeInfoFunc>& _exchangeInfoCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AllOrderBooksFunc(CachedResult<ExchangeInfoFunc>& exchangeInfoCache, CachedResult<MarketsFunc>& marketsCache,
                      CommonInfo& commonInfo)
        : _exchangeInfoCache(exchangeInfoCache), _marketsCache(marketsCache), _commonInfo(commonInfo) {}
#endif

    MarketOrderBookMap operator()(int depth);

    CachedResult<ExchangeInfoFunc>& _exchangeInfoCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CommonInfo& _commonInfo;
  };

  struct OrderBookFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit OrderBookFunc(CommonInfo& commonInfo) : _commonInfo(commonInfo) {}
#endif

    MarketOrderBook operator()(Market m, int depth = kDefaultDepth);

    CommonInfo& _commonInfo;
  };

  struct TradedVolumeFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TradedVolumeFunc(CommonInfo& commonInfo) : _commonInfo(commonInfo) {}
#endif
    MonetaryAmount operator()(Market m);

    CommonInfo& _commonInfo;
  };

  struct TickerFunc {
#ifndef CCT_AGGR_INIT_CXX20
    explicit TickerFunc(CommonInfo& commonInfo) : _commonInfo(commonInfo) {}
#endif

    MonetaryAmount operator()(Market m);

    CommonInfo& _commonInfo;
  };

  CommonInfo _commonInfo;
  CachedResult<ExchangeInfoFunc> _exchangeInfoCache;
  CachedResult<GlobalInfosFunc> _globalInfosCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct