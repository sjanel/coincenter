#pragma once

#include <optional>
#include <type_traits>

#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "curlpostdata.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "depositsconstraints.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "httprequesttype.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "timedef.hpp"
#include "tradeinfo.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

class CoincenterInfo;
class ExchangeConfig;
class FiatConverter;

namespace api {
class APIKey;
class BinancePublic;

class BinancePrivate : public ExchangePrivate {
 public:
  BinancePrivate(const CoincenterInfo& coincenterInfo, BinancePublic& binancePublic, const APIKey& apiKey);

  bool validateApiKey() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(const BalanceOptions& balanceOptions = BalanceOptions()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  ClosedOrderVector queryClosedOrders(const OrdersConstraints& closedOrdersConstraints = OrdersConstraints()) override;

  OpenedOrderVector queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  DepositsSet queryRecentDeposits(const DepositsConstraints& depositsConstraints = DepositsConstraints()) override;

  WithdrawsSet queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints = WithdrawsConstraints()) override;

  MonetaryAmountByCurrencySet queryWithdrawalFees() override { return _allWithdrawFeesCache.get(); }

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) override {
    return _withdrawFeesCache.get(currencyCode);
  }

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

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) override;

  MonetaryAmount queryWithdrawDelivery(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                                       const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  OrderInfo queryOrder(OrderIdView orderId, const TradeContext& tradeContext, HttpRequestType requestType);

  bool checkMarketAppendSymbol(Market mk, CurlPostData& params);

  struct BinanceContext {
    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _exchangePublic;
    Duration& _queryDelay;
  };

  static_assert(std::is_trivially_destructible_v<BinanceContext>, "BinanceContext destructor should be trivial");

  struct TradableCurrenciesCache : public BinanceContext {
    TradableCurrenciesCache(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic,
                            Duration& queryDelay)
        : BinanceContext(curlHandle, apiKey, exchangePublic, queryDelay) {}

    CurrencyExchangeFlatSet operator()();
  };

  struct DepositWalletFunc : public BinanceContext {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic, Duration& queryDelay)
        : BinanceContext(curlHandle, apiKey, exchangePublic, queryDelay) {}

    Wallet operator()(CurrencyCode currencyCode);
  };

  struct AllWithdrawFeesFunc : public BinanceContext {
    AllWithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic,
                        Duration& queryDelay)
        : BinanceContext(curlHandle, apiKey, exchangePublic, queryDelay) {}

    MonetaryAmountByCurrencySet operator()();
  };

  struct WithdrawFeesFunc : public BinanceContext {
    WithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic, Duration& queryDelay)
        : BinanceContext(curlHandle, apiKey, exchangePublic, queryDelay) {}

    std::optional<MonetaryAmount> operator()(CurrencyCode currencyCode);
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesCache> _tradableCurrenciesCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
  CachedResult<AllWithdrawFeesFunc> _allWithdrawFeesCache;
  CachedResult<WithdrawFeesFunc, CurrencyCode> _withdrawFeesCache;
  Duration _queryDelay{};
};
}  // namespace api
}  // namespace cct
