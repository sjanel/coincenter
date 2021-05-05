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
  UpbitPrivate(CoincenterInfo& config, UpbitPublic& upbitPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override { return _tradableCurrenciesCache.get(); }

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  MonetaryAmount trade(MonetaryAmount&, CurrencyCode, const TradeOptions&) override { throw std::runtime_error(""); }

  WithdrawInfo withdraw(MonetaryAmount, ExchangePrivate&) override { throw std::runtime_error(""); }

 private:
  struct TradableCurrenciesFunc {
    TradableCurrenciesFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& upbitPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _upbitPublic(upbitPublic) {}

    CurrencyExchangeFlatSet operator()();

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _upbitPublic;
  };

  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& upbitPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _upbitPublic(upbitPublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _upbitPublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, UpbitPublic& upbitPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _upbitPublic(upbitPublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    UpbitPublic& _upbitPublic;
  };

  json withdrawalInformation(CurrencyCode currencyCode);

  CurlHandle _curlHandle;
  CoincenterInfo& _config;
  UpbitPublic& _upbitPublic;
  CachedResult<TradableCurrenciesFunc> _tradableCurrenciesCache;
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct