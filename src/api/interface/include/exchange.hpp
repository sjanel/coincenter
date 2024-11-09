#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <type_traits>

#include "cache-file-updator-interface.hpp"
#include "cct_exception.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchange-config.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "public-trade-vector.hpp"

namespace cct {

class Exchange : public CacheFileUpdatorInterface {
 public:
  using ExchangePublic = api::ExchangePublic;
  using ExchangePrivate = api::ExchangePrivate;

  /// Builds a Exchange without private exchange. All private requests will be forbidden.
  Exchange(const schema::ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic);

  /// Build a Exchange with both private and public exchanges
  Exchange(const schema::ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic,
           std::unique_ptr<ExchangePrivate> exchangePrivate);

  std::string_view name() const { return apiPublic().name(); }
  ExchangeNameEnum exchangeNameEnum() const { return apiPublic().exchangeNameEnum(); }
  std::string_view keyName() const { return apiPrivate().keyName(); }

  std::size_t publicExchangePos() const;

  ExchangeName createExchangeName() const {
    return ExchangeName(exchangeNameEnum(), hasPrivateAPI() ? keyName() : std::string_view());
  }

  ExchangePublic &apiPublic() { return *_pExchangePublic; }
  const ExchangePublic &apiPublic() const { return *_pExchangePublic; }

  ExchangePrivate &apiPrivate() {
    if (hasPrivateAPI()) {
      return *_exchangePrivate;
    }
    throw exception("No private key associated to exchange {}", name());
  }

  const ExchangePrivate &apiPrivate() const {
    if (hasPrivateAPI()) {
      return *_exchangePrivate;
    }
    throw exception("No private key associated to exchange {}", name());
  }

  const schema::ExchangeConfig &exchangeConfig() const { return *_pExchangeConfig; }

  bool hasPrivateAPI() const { return static_cast<bool>(_exchangePrivate); }

  bool healthCheck() { return apiPublic().healthCheck(); }

  CurrencyExchangeFlatSet queryTradableCurrencies() {
    return hasPrivateAPI() ? _exchangePrivate->queryTradableCurrencies() : apiPublic().queryTradableCurrencies();
  }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) {
    return apiPublic().convertStdCurrencyToCurrencyExchange(currencyCode);
  }

  MarketSet queryTradableMarkets() { return apiPublic().queryTradableMarkets(); }

  MarketPriceMap queryAllPrices() { return apiPublic().queryAllPrices(); }

  MonetaryAmountByCurrencySet queryWithdrawalFees() {
    return hasPrivateAPI() ? _exchangePrivate->queryWithdrawalFees() : apiPublic().queryWithdrawalFees();
  }

  std::optional<MonetaryAmount> queryWithdrawalFee(CurrencyCode currencyCode) {
    return hasPrivateAPI() ? _exchangePrivate->queryWithdrawalFee(currencyCode)
                           : apiPublic().queryWithdrawalFee(currencyCode);
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = ExchangePublic::kDefaultDepth) {
    return apiPublic().queryAllApproximatedOrderBooks(depth);
  }

  MarketOrderBook getOrderBook(Market mk, int depth = ExchangePublic::kDefaultDepth);

  MonetaryAmount queryLast24hVolume(Market mk) { return apiPublic().queryLast24hVolume(mk); }

  /// Retrieve an ordered vector of recent last trades
  PublicTradeVector getLastTrades(Market mk, int nbTrades = ExchangePublic::kNbLastTradesDefault);

  /// Retrieve the last price of given market.
  MonetaryAmount queryLastPrice(Market mk) { return apiPublic().queryLastPrice(mk); }

  bool canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool canDeposit(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool matches(const ExchangeName &exchangeName) const {
    return name() == exchangeName.name() && (!exchangeName.isKeyNameDefined() || keyName() == exchangeName.keyName());
  }

  void updateCacheFile() const override;

  /// unique_ptr is always trivially relocatable whatever the underlying type.
  using trivially_relocatable = std::true_type;

 private:
  ExchangePublic *_pExchangePublic;
  std::unique_ptr<ExchangePrivate> _exchangePrivate;
  const schema::ExchangeConfig *_pExchangeConfig;
};

}  // namespace cct
