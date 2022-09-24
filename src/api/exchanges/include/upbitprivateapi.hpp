#pragma once

#include <span>
#include <unordered_map>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangeprivateapitypes.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class UpbitPublic;

class UpbitPrivate : public ExchangePrivate {
 public:
  UpbitPrivate(const CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  bool canGenerateDepositAddress() const override { return true; }

  Orders queryOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  int cancelOpenedOrders(const OrdersConstraints& openedOrdersConstraints = OrdersConstraints()) override;

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) override {
    return _withdrawalFeesCache.get(currencyCode);
  }

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
  struct TradableCurrenciesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    TradableCurrenciesFunc(CurlHandle& curlHandle, const APIKey& apiKey, const ExchangeInfo& exchangeInfo,
                           CryptowatchAPI& cryptowatchApi)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangeInfo(exchangeInfo), _cryptowatchApi(cryptowatchApi) {}
#endif

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    const ExchangeInfo& _exchangeInfo;
    CryptowatchAPI& _cryptowatchApi;
  };

  struct DepositWalletFunc {
#ifndef CCT_AGGR_INIT_CXX20
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  struct WithdrawFeesFunc {
#ifndef CCT_AGGR_INIT_CXX20
    WithdrawFeesFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& exchangePublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _exchangePublic(exchangePublic) {}
#endif

    MonetaryAmount operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _exchangePublic;
  };

  void applyFee(Market m, CurrencyCode fromCurrencyCode, bool isTakerStrategy, MonetaryAmount& from,
                MonetaryAmount& volume);

  CurlHandle _curlHandle;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
  CachedResult<WithdrawFeesFunc, CurrencyCode> _withdrawalFeesCache;
};
}  // namespace api
}  // namespace cct