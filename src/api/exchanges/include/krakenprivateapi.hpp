#pragma once

#include <span>
#include <string>

#include "cachedresult.hpp"
#include "cct_json.hpp"
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

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) override;

 protected:
  enum class QueryOrder { kOpenedThenClosed, kClosedThenOpened };

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrderInfo(orderId, tradeInfo, QueryOrder::kOpenedThenClosed);
  }

 private:
  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, const CoincenterInfo& config, const APIKey& apiKey,
                       KrakenPublic& exchangePublic)
        : _curlHandle(curlHandle), _config(config), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    const CoincenterInfo& _config;
    const APIKey& _apiKey;
    KrakenPublic& _exchangePublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, KrakenPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    KrakenPublic& _exchangePublic;
  };

  json queryOrdersData(Market m, CurrencyCode fromCurrencyCode, std::string_view userRef, const OrderId& orderId,
                       QueryOrder queryOrder);

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo, QueryOrder queryOrder);

  CurlHandle _curlHandle;
  CurlHandle _placeCancelOrder;  // Kraken has no API limit on place / cancel order, hence a separate CurlHandle
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct