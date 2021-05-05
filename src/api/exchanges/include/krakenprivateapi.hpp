#pragma once

#include <span>
#include <string>

#include "cachedresult.hpp"
#include "cct_json.hpp"
#include "curlhandle.hpp"
#include "exchangeprivateapi.hpp"
#include "tradeinfo.hpp"

namespace cct {

class CoincenterInfo;

namespace api {
class TradeOptions;
class KrakenPublic;

class KrakenPrivate : public ExchangePrivate {
 public:
  KrakenPrivate(CoincenterInfo& config, KrakenPublic& krakenPublic, const APIKey& apiKey);

  CurrencyExchangeFlatSet queryTradableCurrencies() override;

  BalancePortfolio queryAccountBalance(CurrencyCode equiCurrency = CurrencyCode::kNeutral) override {
    return _balanceCache.get(equiCurrency);
  }

  Wallet queryDepositWallet(CurrencyCode currencyCode) override { return _depositWalletsCache.get(currencyCode); }

  MonetaryAmount trade(MonetaryAmount& from, CurrencyCode toCurrencyCode, const TradeOptions& options) override;

  WithdrawInfo withdraw(MonetaryAmount grossAmount, ExchangePrivate& targetExchange) override;

 private:
  struct AccountBalanceFunc {
    AccountBalanceFunc(CurlHandle& curlHandle, CoincenterInfo& config, const APIKey& apiKey, KrakenPublic& krakenPublic)
        : _curlHandle(curlHandle), _config(config), _apiKey(apiKey), _krakenPublic(krakenPublic) {}

    BalancePortfolio operator()(CurrencyCode equiCurrency);

    CurlHandle& _curlHandle;
    CoincenterInfo& _config;
    const APIKey& _apiKey;
    KrakenPublic& _krakenPublic;
  };

  struct DepositWalletFunc {
    DepositWalletFunc(CurlHandle& curlHandle, const APIKey& apiKey, KrakenPublic& krakenPublic)
        : _curlHandle(curlHandle), _apiKey(apiKey), _krakenPublic(krakenPublic) {}

    Wallet operator()(CurrencyCode currencyCode);

    CurlHandle& _curlHandle;
    const APIKey& _apiKey;
    KrakenPublic& _krakenPublic;
  };

  enum class QueryOrder { kOpenedThenClosed, kClosedThenOpened };

  json queryOrdersData(Market m, CurrencyCode fromCurrencyCode, int32_t orderId32Bits,
                       std::span<const std::string> createdOrdersId, QueryOrder queryOrder);

  TradedOrdersInfo queryOrders(Market m, CurrencyCode fromCurrencyCode, int32_t orderId32Bits,
                               std::span<const std::string> createdOrdersId, QueryOrder queryOrder);

  CurlHandle _curlHandle;
  CurlHandle _placeCancelOrder;  // Kraken has no API limit on place / cancel order, hence a separate CurlHandle
  CoincenterInfo& _config;
  KrakenPublic& _krakenPublic;
  CachedResult<AccountBalanceFunc, CurrencyCode> _balanceCache;
  CachedResult<DepositWalletFunc, CurrencyCode> _depositWalletsCache;
};
}  // namespace api
}  // namespace cct