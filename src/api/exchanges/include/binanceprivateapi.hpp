#pragma once

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class BinancePublic;

class BinancePrivate : public ExchangePrivate {
 public:
  BinancePrivate(const CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  void cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  WithdrawalFeeMap queryWithdrawalFees() override { return _allWithdrawFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override { return _withdrawFeesCache.get(currencyCode); }

 protected:
  bool isSimulatedOrderSupported() const override { return true; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderRef& orderRef) override { return queryOrder(orderRef, true); }

  OrderInfo queryOrderInfo(const OrderRef& orderRef) override { return queryOrder(orderRef, false); }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  OrderInfo queryOrder(const OrderRef& orderRef, bool isCancel);

  TradedAmounts queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode, const json& orderJson) const;

  TradedAmounts parseTrades(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const;

  bool checkMarketAppendSymbol(Market m, CurlPostData& params);

  struct TradableCurrenciesCache {
#ifndef CCT_AGGR_INIT_CXX20
    TradableCurrenciesCache(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& binancePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _public(binancePublic) {}
#endif

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _public;
  };

  struct DepositWalletFunc {
#ifndef CCT_AGGR_INIT_CXX20
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& binancePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _public(binancePublic) {}
#endif

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _public;
  };

  struct AllWithdrawFeesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AllWithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    WithdrawalFeeMap operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _exchangePublic;
  };

  struct WithdrawFeesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    WithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    MonetaryAmount operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _exchangePublic;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesCache> _tradableCurrenciesCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
  CachedResult<AllWithdrawFeesFunc> _allWithdrawFeesCache;
  CachedResult<WithdrawFeesFunc, CurrencyCode> _withdrawFeesCache;
};
}  // namespace api
}  // namespace cct