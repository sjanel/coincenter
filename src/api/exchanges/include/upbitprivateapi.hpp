#pragma once

#include <span>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class TradeOptions;
class UpbitPublic;

class UpbitPrivate : public ExchangePrivate {
 public:
  UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  WithdrawInfo withdraw(MonetaryAmount, ExchangePrivate&) override { throw std::runtime_error(""); }

 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override;

 private:
  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  json withdrawalInformation(CurrencyCode currencyCode);

  OrderInfo parseOrderJson(const json& orderJson, CurrencyCode fromCurrencyCode, Market m) const;

  bool isOrderClosed(const json& orderJson) const;
  bool isOrderTooSmall(MonetaryAmount volume, MonetaryAmount price) const;

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct