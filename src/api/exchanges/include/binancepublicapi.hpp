#pragma once

#include <optional>
#include <string_view>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "permanentcurloptions.hpp"
#include "public-trade-vector.hpp"
#include "runmodes.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeConfig;
class FiatConverter;

namespace api {
class CommonAPI;

class BinancePublic : public ExchangePublic {
 public:
  static constexpr std::string_view kURLBases[] = {"https://api.binance.com", "https://api1.binance.com",
                                                   "https://api2.binance.com", "https://api3.binance.com"};

  BinancePublic(const CoincenterInfo& coincenterInfo, FiatConverter& fiatConverter, api::CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override {
    return queryTradableCurrencies(_globalInfosCache.get());
  }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode standardCode) override {
    return *queryTradableCurrencies().find(standardCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

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

  MonetaryAmount sanitizePrice(Market mk, MonetaryAmount pri);

  MonetaryAmount sanitizeVolume(Market mk, MonetaryAmount vol, MonetaryAmount priceForNotional, bool isTakerOrder);

 private:
  friend class BinancePrivate;

  CurrencyExchangeFlatSet queryTradableCurrencies(const json& data) const;

  MonetaryAmount computePriceForNotional(Market mk, int avgPriceMins);

  struct CommonInfo {
    CommonInfo(const CoincenterInfo& coincenterInfo, const ExchangeConfig& exchangeConfig, settings::RunMode runMode);

    const ExchangeConfig& _exchangeConfig;
    CurlHandle _curlHandle;
  };

  struct ExchangeInfoFunc {
    using ExchangeInfoDataByMarket = std::unordered_map<Market, json>;

    ExchangeInfoDataByMarket operator()();

    CommonInfo& _commonInfo;
  };

  struct GlobalInfosFunc {
    GlobalInfosFunc(AbstractMetricGateway* pMetricGateway, const PermanentCurlOptions& permanentCurlOptions,
                    settings::RunMode runMode);

    json operator()();

    CurlHandle _curlHandle;
  };

  struct MarketsFunc {
    MarketSet operator()();

    CachedResult<ExchangeInfoFunc>& _exchangeConfigCache;
    CurlHandle& _curlHandle;
    const ExchangeConfig& _exchangeConfig;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    CachedResult<ExchangeInfoFunc>& _exchangeConfigCache;
    CachedResult<MarketsFunc>& _marketsCache;
    CommonInfo& _commonInfo;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int depth = kDefaultDepth);

    CommonInfo& _commonInfo;
  };

  struct TradedVolumeFunc {
    MonetaryAmount operator()(Market mk);

    CommonInfo& _commonInfo;
  };

  struct TickerFunc {
    MonetaryAmount operator()(Market mk);

    CommonInfo& _commonInfo;
  };

  CommonInfo _commonInfo;
  CachedResult<ExchangeInfoFunc> _exchangeConfigCache;
  CachedResult<GlobalInfosFunc> _globalInfosCache;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct
