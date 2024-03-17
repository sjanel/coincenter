#pragma once

#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "timedef.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class APIKey;
class BithumbPublic;

class BithumbPrivate : public ExchangePrivate {
 public:
  BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey);

  bool validateApiKey() override;

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(const BalanceOptions& balanceOptions = BalanceOptions()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return false; }

  ClosedOrderVector queryClosedOrders(const OrdersConstraints& closedOrdersConstraints = OrdersConstraints()) override;

  OpenedOrderVector queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  DepositsSet queryRecentDeposits(const DepositsConstraints& depositsConstraints = DepositsConstraints()) override;

  WithdrawsSet queryRecentWithdraws(const WithdrawsConstraints& withdrawsConstraints = WithdrawsConstraints()) override;

  void updateCacheFile() const override;

 protected:
  bool isSimulatedOrderSupported() const override { return false; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(OrderIdView orderId, const TradeContext& tradeContext) override;

  OrderInfo queryOrderInfo(OrderIdView orderId, const TradeContext& tradeContext) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& destinationWallet) override;

 private:
  friend class BithumbPrivateAPIPlaceOrderTest;

  struct DepositWalletFunc {
    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BithumbPublic& _exchangePublic;
  };

  void cancelOrderProcess(OrderIdView orderId, const TradeContext& tradeContext);

  struct CurrencyOrderInfo {
    int8_t nbDecimals{};
    TimePoint lastNbDecimalsUpdatedTime;
    MonetaryAmount minOrderSize;
    TimePoint lastMinOrderSizeUpdatedTime;
    MonetaryAmount minOrderPrice;
    TimePoint lastMinOrderPriceUpdatedTime;
    MonetaryAmount maxOrderPrice;
    TimePoint lastMaxOrderPriceUpdatedTime;
  };

  using CurrencyOrderInfoMap = std::unordered_map<CurrencyCode, CurrencyOrderInfo>;

  CurlHandle _curlHandle;
  CurrencyOrderInfoMap _currencyOrderInfoMap;
  Duration _currencyOrderInfoRefreshTime;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct