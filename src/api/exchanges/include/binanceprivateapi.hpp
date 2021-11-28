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

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  WithdrawalFeeMap queryWithdrawalFees() override { return _allWithdrawFeesCache.get(); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override { return _withdrawFeesCache.get(currencyCode); }

 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrder(orderId, tradeInfo, true);
  }

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override {
    return queryOrder(orderId, tradeInfo, false);
  }

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  OrderInfo queryOrder(const OrderId& orderId, const TradeInfo& tradeInfo, bool isCancel);

  TradedAmounts queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode, const json& orderJson) const;

  TradedAmounts parseTrades(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const;

  struct TradableCurrenciesCache {
    TradableCurrenciesCache(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& binancePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _public(binancePublic) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _public;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& binancePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _public(binancePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _public;
  };

  struct AllWithdrawFeesFunc {
    AllWithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    WithdrawalFeeMap operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _exchangePublic;
  };

  struct WithdrawFeesFunc {
    WithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, BinancePublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    MonetaryAmount operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    BinancePublic& _exchangePublic;
  };

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesCache> _tradableCurrenciesCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
  CachedResult<AllWithdrawFeesFunc> _allWithdrawFeesCache;
  CachedResult<WithdrawFeesFunc, CurrencyCode> _withdrawFeesCache;
};
}  // namespace api
}  // namespace cct