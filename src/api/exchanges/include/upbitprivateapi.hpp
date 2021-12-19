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
class UpbitPublic;

class UpbitPrivate : public ExchangePrivate {
 public:
  UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get(currencyCode);
  }

 protected:
  bool isSimulatedOrderSupported() const override { return false; }

  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(CurlHandle& curlHandle, const APIKey& apiKey, const ExchangeInfo& exchangeInfo,
                           CryptowatchAPI& cryptowatchApi)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangeInfo(exchangeInfo), _cryptowatchApi(cryptowatchApi) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    const ExchangeInfo& _exchangeInfo;
    CryptowatchAPI& _cryptowatchApi;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  struct WithdrawFeesFunc {
    WithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}

    MonetaryAmount operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  OrderInfo parseOrderJson(const json& orderJson, CurrencyCode fromCurrencyCode, Market m) const;

  bool isOrderClosed(const json& orderJson) const;
  bool isOrderTooSmall(MonetaryAmount volume, MonetaryAmount price) const;

  MonetaryAmount sanitizeVolume(MonetaryAmount volume, MonetaryAmount price) const;

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
  CachedResult<WithdrawFeesFunc, CurrencyCode> _withdrawalFeesCache;
};
}  // namespace api
}  // namespace cct