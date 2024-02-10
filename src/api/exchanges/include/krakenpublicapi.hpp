#pragma once

#include <optional>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "static_string_view_helpers.hpp"
#include "timedef.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeConfig;

namespace api {
class CommonAPI;

class KrakenPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kExchangeName = "kraken";

  KrakenPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get().first; }

  MonetaryAmount queryVolumeOrderMin(Market mk) { return _marketsCache.get().second.find(mk)->second.minVolumeOrder; }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override {
    return _commonApi.queryWithdrawalFees(kExchangeName).first;
  }

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return false; }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get(depth);
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) override {
    return _orderBookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tickerCache.get(mk).first; }

  LastTradesVector queryLastTrades(Market mk, int nbLastTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override { return _tickerCache.get(mk).second; }

  static constexpr std::string_view kUrlPrefix = "https://api.kraken.com";
  static constexpr std::string_view kVersion = "/0";
  static constexpr std::string_view kUrlBase = JoinStringView_v<kUrlPrefix, kVersion>;

 private:
  friend class KrakenPrivate;

  struct TradableCurrenciesFunc {
    CurrencyExchangeFlatSet operator()();

    const CoincenterInfo& _coincenterInfo;
    CommonAPI& _commonApi;
    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct MarketsFunc {
    struct MarketInfo {
      VolAndPriNbDecimals volAndPriNbDecimals;
      MonetaryAmount minVolumeOrder;
    };

    using MarketInfoMap = std::unordered_map<Market, MarketInfo>;

    std::pair<MarketSet, MarketInfoMap> operator()();

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int count);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CurlHandle& _curlHandle;
  };

  struct TickerFunc {
    using Last24hTradedVolumeAndLatestPricePair = std::pair<MonetaryAmount, MonetaryAmount>;

    Last24hTradedVolumeAndLatestPricePair operator()(Market mk);

    CachedResult<TradableCurrenciesFunc>& _tradableCurrenciesCache;
    CurlHandle& _curlHandle;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderBookCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};
}  // namespace api
}  // namespace cct
