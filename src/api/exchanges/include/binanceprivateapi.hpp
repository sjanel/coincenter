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
  BinancePrivate(CoincenterInfo& config, BinancePublic& binancePublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override;

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  MonetaryAmount trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) override;

  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) override;

 private:
  TradedOrdersInfo queryOrdersAfterPlace(Market m, CurrencyCode fromCurrencyCode, const json& orderJson) const;

  TradedOrdersInfo queryOrder(Market m, CurrencyCode fromCurrencyCode, const json& fillDetail) const;

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
  CoincenterInfo& _config;
  BinancePublic& _public;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct