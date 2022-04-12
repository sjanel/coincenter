#pragma once

#include <span>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class KrakenPublic;

class KrakenPrivate : public ExchangePrivate {
 public:
  KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  void cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

 protected:
  bool isSimulatedOrderSupported() const override { return true; }

  enum class QueryOrder { kOpenedThenClosed, kClosedThenOpened };

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderRef& orderRef) override;

  OrderInfo queryOrderInfo(const OrderRef& orderRef) override {
    return queryOrderInfo(orderRef, QueryOrder::kOpenedThenClosed);
  }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct DepositWalletFunc {
#ifndef CCT_AGGR_INIT_CXX20
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, KrakenPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    KrakenPublic& _exchangePublic;
  };

  json queryOrdersData(int64_t userRef, const OrderId& orderId, QueryOrder queryOrder);

  OrderInfo queryOrderInfo(const OrderRef& orderRef, QueryOrder queryOrder);

  void cancelOrderProcess(const OrderId& id);

  CurlHandle _curlHandle;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct