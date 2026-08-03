#pragma once

#include <optional>
#include <string_view>

#include "cachedresult.hpp"
#include "cct_string.hpp"
#include "httpclient.hpp"
#include "currencycodeset.hpp"
#include "exchange-asset-config.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "public-trade-vector.hpp"

namespace cct {

class CoincenterInfo;
class FiatConverter;

namespace api {
class CommonAPI;

class UpbitPublic : public ExchangePublic {
 public:
  static string ReverseMarketStr(Market mk) { return mk.reverse().assetsPairStrUpper('-'); }

  UpbitPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override { return _marketsCache.get(); }

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get(1)); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

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

  static constexpr std::string_view kUrlBase = "https://api.upbit.com";

 private:
  friend class UpbitPrivate;

  static bool CheckCurrencyCode(CurrencyCode standardCode, const CurrencyCodeSet& excludedCurrencies);

  static MonetaryAmount SanitizeVolume(MonetaryAmount vol, MonetaryAmount pri);

  struct MarketsFunc {
    MarketSet operator()();

    HttpClient& _httpClient;
    const schema::ExchangeAssetConfig& _assetConfig;
  };

  struct TradableCurrenciesFunc {
    CurrencyExchangeFlatSet operator()();

    HttpClient& _httpClient;
    CachedResult<MarketsFunc>& _marketsCache;
  };

  struct WithdrawalFeesFunc {
    MonetaryAmountByCurrencySet operator()() const;

    std::string_view _name;
    std::string_view _dataDir;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()(int depth);

    HttpClient& _httpClient;
    CachedResult<MarketsFunc>& _marketsCache;
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

  HttpClient _httpClient;
  CachedResult<MarketsFunc> _marketsCache;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc, int> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
  CachedResult<TickerFunc, Market> _tickerCache;
};

}  // namespace api
}  // namespace cct
