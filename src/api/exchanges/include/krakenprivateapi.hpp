#pragma once

#include "cachedresult.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "kraken-schema.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class KrakenPublic;

class KrakenPrivate : public ExchangePrivate {
 public:
  KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey);

  bool validateApiKey() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(const BalanceOptions& balanceOptions = BalanceOptions()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  ClosedOrderVector queryClosedOrders(const OrdersConstraints& closedOrdersConstraints = OrdersConstraints()) override;

  OpenedOrderVector queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  DepositsSet queryRecentDeposits(const DepositsConstraints& depositsConstraints = DepositsConstraints()) override;

  WithdrawsSet queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints = WithdrawsConstraints()) override;

 protected:
  bool isSimulatedOrderSupported() const override { return true; }

  enum class QueryOrder { kOpenedThenClosed, kClosedThenOpened };

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) override;

  OrderInfo queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) override {
    return queryOrderInfo(orderId, tradeContext, QueryOrder::kOpenedThenClosed);
  }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) override;

 private:
  struct DepositWalletFunc {
    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    KrakenPublic& _exchangePublic;
  };

  schema::kraken::OpenedOrClosedOrders queryOrdersData(int64_t userRef, OrderIdView orderId, QueryOrder queryOrder);

  OrderInfo queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext, QueryOrder queryOrder);

  void cancelOrderProcess(OrderIdView orderId);

  CurlHandle _curlHandle;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct