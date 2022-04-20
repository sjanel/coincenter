#pragma once

#include <unordered_map>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "timedef.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {

class BithumbPublic;

class BithumbPrivate : public ExchangePrivate {
 public:
  BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return false; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  void updateCacheFile() const override;

 protected:
  bool isSimulatedOrderSupported() const override { return false; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderRef& orderRef) override;

  OrderInfo queryOrderInfo(const OrderRef& orderRef) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct DepositWalletFunc {
#ifndef CCT_AGGR_INIT_CXX20
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, BithumbPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BithumbPublic& _exchangePublic;
  };

  void cancelOrderProcess(const OrderRef& orderRef);

  struct CurrencyOrderInfo {
    int8_t nbDecimals{};
    TimePoint lastNbDecimalsUpdatedTime{};
    MonetaryAmount minOrderSize;
    TimePoint lastMinOrderSizeUpdatedTime{};
    MonetaryAmount minOrderPrice;
    TimePoint lastMinOrderPriceUpdatedTime{};
    MonetaryAmount maxOrderPrice;
    TimePoint lastMaxOrderPriceUpdatedTime{};
  };

  using CurrencyOrderInfoMap = std::unordered_map<CurrencyCode, CurrencyOrderInfo>;

  CurlHandle _curlHandle;
  CurrencyOrderInfoMap _currencyOrderInfoMap;
  Duration _currencyOrderInfoRefreshTime;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct