#pragma once

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "curloptions.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapitypes.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class BinancePublic;

class BinancePrivate : public ExchangePrivate {
 public:
  BinancePrivate(const CoincenterInfo& coincenterInfo, BinancePublic& binancePublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(const BalanceOptions& balanceOptions = BalanceOptions()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  Deposits queryRecentDeposits(const DepositsConstraints& depositsConstraints = DepositsConstraints()) override;

  WithdrawalFeeMap queryWithdrawalFees() override { return _allWithdrawFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override { return _withdrawFeesCache.get(currencyCode); }

 protected:
  bool isSimulatedOrderSupported() const override { return true; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) override {
    return queryOrder(orderId, tradeContext, HttpRequestType::kDelete);
  }

  OrderInfo queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) override {
    return queryOrder(orderId, tradeContext, HttpRequestType::kGet);
  }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  ReceivedWithdrawInfo isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  OrderInfo queryOrder(OrderIdView orderId, const TradeContext& tradeContext, HttpRequestType requestType);

  bool checkMarketAppendSymbol(Market mk, CurlPostData& params);

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
