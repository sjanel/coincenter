#pragma once

#include <chrono>
#include <unordered_map>

#include "cachedresult.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class TradeOptions;
class BithumbPublic;

class BithumbPrivate : public ExchangePrivate {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  struct NbDecimalsTimeValue {
    int8_t nbDecimals;
    TimePoint lastUpdatedTime;
  };

  using MaxNbDecimalsUnitMap = std::unordered_map<CurrencyCode, NbDecimalsTimeValue>;

  BithumbPrivate(const CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode()) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  void updateCacheFile() const override;

 protected:
  PlaceOrderInfo placeOrder(MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price, const TradeInfo& tradeInfo) override;

  OrderInfo cancelOrder(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  OrderInfo queryOrderInfo(const OrderId& orderId, const TradeInfo& tradeInfo) override;

  InitiatedWithdrawInfo launchWithdraw(MonetaryAmount grossAmount, Wallet&& wallet) override;

  SentWithdrawInfo isWithdrawSuccessfullySent(const InitiatedWithdrawInfo& initiatedWithdrawInfo) override;

  bool isWithdrawReceived(const InitiatedWithdrawInfo& initiatedWithdrawInfo,
                          const SentWithdrawInfo& sentWithdrawInfo) override;

 private:
  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, const APIKey& apiKey, MaxNbDecimalsUnitMap& maxNbDecimalsUnitMap,
                       BithumbPublic& exchangePublic)
        : _curlHandle(curlHandle),
          _apiKey(apiKey),
          _maxNbDecimalsUnitMap(maxNbDecimalsUnitMap),
          _exchangePublic(exchangePublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    MaxNbDecimalsUnitMap& _maxNbDecimalsUnitMap;
    BithumbPublic& _exchangePublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, MaxNbDecimalsUnitMap& maxNbDecimalsUnitMap,
                      BithumbPublic& exchangePublic)
        : _curlHandle(curlHandle),
          _apiKey(apiKey),
          _maxNbDecimalsUnitMap(maxNbDecimalsUnitMap),
          _exchangePublic(exchangePublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    MaxNbDecimalsUnitMap& _maxNbDecimalsUnitMap;
    BithumbPublic& _exchangePublic;
  };

  CurlHandle _curlHandle;
  MaxNbDecimalsUnitMap _maxNbDecimalsPerCurrencyCodePlace;
  Clock::duration _nbDecimalsRefreshTime;
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct