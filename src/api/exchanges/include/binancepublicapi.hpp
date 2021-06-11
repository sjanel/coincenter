#pragma once

#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
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
class TradeOptions;

class BinancePublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.binance.com", "https://api1.binance.com",
                                                   "https://api2.binance.com", "https://api3.binance.com"};

  static constexpr char kUserAgent[] = "Binance C++ API Client";

  BinancePublic(CoincenterInfo& config, FiatConverter& fiatConverter, api::CryptowatchAPI& cryptowatchAPI);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return marketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  WithdrawalFeeMap queryWithdrawalFees() override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = kDefaultDepth) override { return _orderbookCache.get(m, depth); }

  MonetaryAmount sanitizePrice(Market m, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market m, MonetaryAmount vol, MonetaryAmount sanitizedPrice, bool isTakerOrder);

 private:
  friend class BinancePrivate;

  class CommonInfo {
   public:
    static constexpr auto kNbBaseURLs = std::distance(std::begin(kURLBases), std::end(kURLBases));

    CommonInfo(const ExchangeInfo& exchangeInfo, settings::RunMode runMode);

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

    ExchangeInfoFunc(CoincenterInfo& config, CommonInfo& commonInfo) : _config(config), _commonInfo(commonInfo) {}

    ExchangeInfoDataByMarket operator()();

    CoincenterInfo& _config;
    CommonInfo& _commonInfo;
  };

  struct GlobalInfosFunc {
    json operator()();

    CurlHandle _curlHandle;
  };

  struct MarketsFunc {
    MarketsFunc(CachedResult<ExchangeInfoFunc>& exchangeInfoCache, CurlHandle& curlHandle,
                const ExchangeInfo& exchangeInfo)
        : _exchangeInfoCache(exchangeInfoCache), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}

    MarketSet operator()();

    CachedResult<ExchangeInfoFunc>& _exchangeInfoCache;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct AllOrderBooksFunc {
    AllOrderBooksFunc(CachedResult<ExchangeInfoFunc>& exchangeInfoCache, CachedResult<MarketsFunc>& marketsCache,
                      CommonInfo& commonInfo)
        : _exchangeInfoCache(exchangeInfoCache), _marketsCache(marketsCache), _commonInfo(commonInfo) {}

    MarketOrderBookMap operator()(int depth);

    CachedResult<ExchangeInfoFunc>& _exchangeInfoCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CommonInfo& _commonInfo;
  };

  struct OrderBookFunc {
    OrderBookFunc(CoincenterInfo& config, CommonInfo& commonInfo) : _config(config), _commonInfo(commonInfo) {}

    MarketOrderBook operator()(Market m, int depth = kDefaultDepth);

    CoincenterInfo& _config;
    CommonInfo& _commonInfo;
  };

  CommonInfo _commonInfo;
  CachedResult<ExchangeInfoFunc> _exchangeInfoCache;
  CachedResult<GlobalInfosFunc> _globalInfosCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
};

}  // namespace api
}  // namespace cct