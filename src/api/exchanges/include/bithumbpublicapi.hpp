#pragma once

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "timedef.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class CommonAPI;

class BithumbPublic : public ExchangePublic {
 public:
  static constexpr std::string_view kStatusOKStr = "0000";

  BithumbPublic(const CoincenterInfo& config, FiatConverter& fiatConverter, CommonAPI& commonAPI);

  bool healthCheck() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) override {
    return _tradableCurrenciesCache.get().getOrThrow(currencyCode);
  }

  MarketSet queryTradableMarkets() override;

  MarketPriceMap queryAllPrices() override { return MarketPriceMapFromMarketOrderBookMap(_allOrderBooksCache.get()); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _withdrawalFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override;

  bool isWithdrawalFeesSourceReliable() const override { return true; }

  MarketOrderBookMap queryAllApproximatedOrderBooks([[maybe_unused]] int depth = kDefaultDepth) override {
    return _allOrderBooksCache.get();
  }

  MarketOrderBook queryOrderBook(Market mk, int depth = kDefaultDepth) override {
    return _orderbookCache.get(mk, depth);
  }

  MonetaryAmount queryLast24hVolume(Market mk) override { return _tradedVolumeCache.get(mk); }

  LastTradesVector queryLastTrades(Market mk, int nbTrades = kNbLastTradesDefault) override;

  MonetaryAmount queryLastPrice(Market mk) override;

  static constexpr std::string_view kUrlBase = "https://api.bithumb.com";

 private:
  friend class BithumbPrivate;

  struct TradableCurrenciesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    TradableCurrenciesFunc(const CoincenterInfo& config, CommonAPI& commonAPI, CurlHandle& curlHandle)
        : _coincenterInfo(config), _commonAPI(commonAPI), _curlHandle(curlHandle) {}
#endif

    CurrencyExchangeFlatSet operator()();

    const CoincenterInfo& _coincenterInfo;
    CommonAPI& _commonAPI;
    CurlHandle& _curlHandle;
  };

  struct WithdrawalFeesFunc {
    static constexpr std::string_view kFeeUrl = "https://www.bithumb.com";

    WithdrawalFeesFunc(AbstractMetricGateway* pMetricGateway, const PermanentCurlOptions& permanentCurlOptions,
                       settings::RunMode runMode)
        : _curlHandle(kFeeUrl, pMetricGateway, permanentCurlOptions, runMode) {}

    WithdrawalFeeMap operator()();

    CurlHandle _curlHandle;
  };

  struct AllOrderBooksFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AllOrderBooksFunc(const CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _coincenterInfo(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

    MarketOrderBookMap operator()();

    const CoincenterInfo& _coincenterInfo;
    CurlHandle& _curlHandle;
    const ExchangeInfo& _exchangeInfo;
  };

  struct OrderBookFunc {
#ifndef CCT_AGGR_INIT_CXX20
    OrderBookFunc(const CoincenterInfo& config, CurlHandle& curlHandle, const ExchangeInfo& exchangeInfo)
        : _coincenterInfo(config), _curlHandle(curlHandle), _exchangeInfo(exchangeInfo) {}
#endif

    MarketOrderBook operator()(Market mk, int depth);

    const CoincenterInfo& _coincenterInfo;
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

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<WithdrawalFeesFunc> _withdrawalFeesCache;
  CachedResult<AllOrderBooksFunc> _allOrderBooksCache;
  CachedResult<OrderBookFunc, Market, int> _orderbookCache;
  CachedResult<TradedVolumeFunc, Market> _tradedVolumeCache;
};

}  // namespace api
}  // namespace cct