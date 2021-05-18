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
class HuobiPublic;

class HuobiPrivate : public ExchangePrivate {
 public:
  HuobiPrivate(const CoincenterInfo& config, HuobiPublic& huobiPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _exchangePublic.queryTradableCurrencies(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price,
                            const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct AccountIdFunc {
    AccountIdFunc(CurlHandle& curlHandle, const APIKey& apiKey) : _curlHandle(curlHandle), _apiKey(apiKey) {}

    int operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, const HuobiPublic& huobiPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _huobiPublic(huobiPublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    const HuobiPublic& _huobiPublic;
  };

  CurlHandle _curlHandle;
  CachedResult<AccountIdFunc> _accountIdCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct