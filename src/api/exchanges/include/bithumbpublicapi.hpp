#pragma once

#include <optional>
#include <string_view>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "currencyexchange.hpp"
#include "exchange-asset-config.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "public-trade-vector.hpp"

namespace cct {

class CoincenterInfo;
class FiatConverter;

namespace api {
class CommonAPI;

class BithumbPublic : public ExchangePublic {
 public:
  static constexpr auto kStatusOK = 0;
  static constexpr auto kStatusUnexpectedError = -1;
  static constexpr auto kStatusNotPresentError = -2;

  BithumbPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override;

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get()); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() override {
    return _commonApi.tryQueryWithdrawalFees(exchangeNameEnum());
  }

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return false; }

  MarketOrderBookMap queryAllApproximatedOrderBooks([[maybe_unused]] int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get();
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) override {
    return _orderbookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  PublicTradeVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override;

  static constexpr std::string_view kUrlBase = "https://api.bithumb.com";

 private:
  friend class BithumbPrivate;

  struct TradableCurrenciesFunc {
    CurrencyExchangeFlatSet operator()();

    const CoincenterInfo& _coincenterInfo;
    CommonAPI& _commonAPI;
    CurlHandle& _curlHandle;
  };

  struct AllOrderBooksFunc {
    MarketOrderBookMap operator()();

    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const schema::ExchangeAssetConfig& _assetConfig;
  };

  struct OrderBookFunc {
    MarketOrderBook operator()(Market mk, int depth);

    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const schema::ExchangeAssetConfig& _assetConfig;
  };

  struct TradedVolumeFunc {
    MonetaryAmount operator()(Market mk);

    CurlHandle& _curlHandle;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<AllOrderBooksFunc> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
};

}  // namespace api
}  // namespace cct
