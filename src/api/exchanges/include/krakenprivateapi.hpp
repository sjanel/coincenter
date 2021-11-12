#pragma once

#include <span>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class TradeOptions;
class KrakenPublic;

class KrakenPrivate : public ExchangePrivate {
 public:
  KrakenPrivate(const CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

 protected:
  enum class QueryOrder { kOpenedThenClosed, kClosedThenOpened };

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrderInfo(orderId, tradeInfo, QueryOrder::kOpenedThenClosed);
  }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, KrakenPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    KrakenPublic& _exchangePublic;
  };

  json queryOrdersData(Market m, CurrencyCode fromCurrencyCode, int64_t userRef, const OrderId& orderId,
                       QueryOrder queryOrder);

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo, QueryOrder queryOrder);

  CurlHandle _curlHandle;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct