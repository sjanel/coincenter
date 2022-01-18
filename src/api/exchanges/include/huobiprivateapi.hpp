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
class HuobiPublic;

class HuobiPrivate : public ExchangePrivate {
 public:
  HuobiPrivate(const CoincenterInfo& config, HuobiPublic& huobiPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  void cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

 protected:
  bool isSimulatedOrderSupported() const override { return false; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderRef& orderRef) override;

  void batchCancel(const OrdersConstraints::OrderIdSet& orderIdSet);

  OrderInfo queryOrderInfo(const OrderRef& orderRef) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  void cancelOrderProcess(const OrderId& id);

  struct AccountIdFunc {
#ifndef CCT_AGGR_INIT_CXX20
    AccountIdFunc(CurlHandle& curlHandle, const APIKey& apiKey) : _curlHandle(curlHandle), _apiKey(apiKey) {}
#endif

    int operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
  };

  struct DepositWalletFunc {
#ifndef CCT_AGGR_INIT_CXX20
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, const HuobiPublic& huobiPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _huobiPublic(huobiPublic) {}
#endif

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