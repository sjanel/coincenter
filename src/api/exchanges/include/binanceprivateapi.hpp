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
class BinancePublic;

class BinancePrivate : public ExchangePrivate {
 public:
  BinancePrivate(const CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) override;

 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price, const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrder(orderId, tradeInfo, true);
  }

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrder(orderId, tradeInfo, false);
  }

 private:
  OrderInfo queryOrder(const OrderId& orderId, const TradeInfo& tradeInfo, bool isCancel);

  TradedAmounts queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode, const json& orderJson) const;

  TradedAmounts queryOrder(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const;

  void updateRemainingVolume(Market m, const json& result, MonetaryAmount& remFrom) const;

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& binancePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _public(binancePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _public;
  };

  CurlHandle _curlHandle;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct