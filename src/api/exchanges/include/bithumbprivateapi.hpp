#pragma once

#include <chrono>
#include <span>
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

  BithumbPrivate(CoincenterInfo& config, BithumbPublic& bithumbPublic, const APIKey& apiKey);

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  MonetaryAmount trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) override;

  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) override;

  void updateCacheFile() const override;

 private:
  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, const APIKey& apiKey, MaxNbDecimalsUnitMap& maxNbDecimalsUnitMap,
                       BithumbPublic& bithumbPublic)
        : _curlHandle(curlHandle),
          _apiKey(apiKey),
          _maxNbDecimalsUnitMap(maxNbDecimalsUnitMap),
          _bithumbPublic(bithumbPublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    MaxNbDecimalsUnitMap& _maxNbDecimalsUnitMap;
    BithumbPublic& _bithumbPublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, MaxNbDecimalsUnitMap& maxNbDecimalsUnitMap,
                      BithumbPublic& bithumbPublic)
        : _curlHandle(curlHandle),
          _apiKey(apiKey),
          _maxNbDecimalsUnitMap(maxNbDecimalsUnitMap),
          _bithumbPublic(bithumbPublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    MaxNbDecimalsUnitMap& _maxNbDecimalsUnitMap;
    BithumbPublic& _bithumbPublic;
  };

  TradedOrdersInfo queryClosedOrders(Market m, CurrencyCode fromCurrencyCode,
                                     std::span<const std::string> createdOrdersId);

  CurlHandle _curlHandle;
  MaxNbDecimalsUnitMap _maxNbDecimalsPerCurrencyCodePlace;
  Clock::duration _nbDecimalsRefreshTime;
  CoincenterInfo& _config;
  BithumbPublic& _bithumbPublic;
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct