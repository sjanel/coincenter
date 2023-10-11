#pragma once

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeInfo;
class FiatConverter;

namespace api {
class HuobiPublic;

class HuobiPrivate : public ExchangePrivate {
 public:
  HuobiPrivate(const CoincenterInfo& coincenterInfo, HuobiPublic& huobiPublic, const APIKey& apiKey);

  bool validateApiKey() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(const BalanceOptions& balanceOptions = BalanceOptions()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  DepositsSet queryRecentDeposits(const DepositsConstraints& depositsConstraints = DepositsConstraints()) override;

  WithdrawsSet queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints = WithdrawsConstraints()) override;

 protected:
  bool isSimulatedOrderSupported() const override { return false; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) override;

  int batchCancel(const OrdersConstraints::OrderIdSet& orderIdSet);

  OrderInfo queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) override;

 private:
  void cancelOrderProcess(OrderIdView orderId);

  struct AccountIdFunc {
    int operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
  };

  struct DepositWalletFunc {
    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    const HuobiPublic& _huobiPublic;
  };

  CurlHandle _curlHandle;
  CachedResult<AccountIdFunc> _accountIdCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct