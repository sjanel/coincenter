#pragma once

#include <unordered_map>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "timehelpers.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {

class BithumbPublic;

class BithumbPrivate : public ExchangePrivate {
 public:
  struct NbDecimalsTimeValue {
    int8_t nbDecimals;
    TimePoint lastUpdatedTime;
  };

  using MaxNbDecimalsUnitMap = std::unordered_map<CurrencyCode, NbDecimalsTimeValue>;

  BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return false; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  void cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

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
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, MaxNbDecimalsUnitMap& maxNbDecimalsUnitMap,
                      BithumbPublic& exchangePublic)
        : _curlHandle(curlHandle),
          _apiKey(apiKey),
          _maxNbDecimalsUnitMap(maxNbDecimalsUnitMap),
          _exchangePublic(exchangePublic) {}
#endif

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    MaxNbDecimalsUnitMap& _maxNbDecimalsUnitMap;
    BithumbPublic& _exchangePublic;
  };

  void cancelOrderProcess(const OrderRef& orderRef);

  CurlHandle _curlHandle;
  MaxNbDecimalsUnitMap _maxNbDecimalsUnitMap;
  Duration _nbDecimalsRefreshTime;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct