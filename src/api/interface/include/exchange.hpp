#pragma once

#include <string_view>

#include "exchangeinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"

namespace cct {
class Exchange {
 public:
  using ExchangePublic = api::ExchangePublic;
  using WithdrawalFeeMap = api::ExchangePublic::WithdrawalFeeMap;
  using MarketSet = api::ExchangePublic::MarketSet;
  using MarketPriceMap = api::ExchangePublic::MarketPriceMap;
  using MarketOrderBookMap = api::ExchangePublic::MarketOrderBookMap;
  using LastTradesVector = api::ExchangePublic::LastTradesVector;

  /// Builds a Exchange without private exchange. All private requests will be forbidden.
  Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic);

  /// Build a Exchange with both private and public exchanges
  Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic,
           api::ExchangePrivate &exchangePrivate);

  std::string_view name() const { return _exchangePublic.name(); }
  std::string_view keyName() const { return apiPrivate().keyName(); }

  api::ExchangePublic &apiPublic() { return _exchangePublic; }
  const api::ExchangePublic &apiPublic() const { return _exchangePublic; }

  api::ExchangePrivate &apiPrivate() {
    if (_pExchangePrivate) {
      return *_pExchangePrivate;
    }
    throw exception("No private key associated to this exchange");
  }

  const api::ExchangePrivate &apiPrivate() const {
    if (_pExchangePrivate) {
      return *_pExchangePrivate;
    }
    throw exception("No private key associated to this exchange");
  }

  const ExchangeInfo &exchangeInfo() const { return _exchangeInfo; }

  bool hasPrivateAPI() const { return _pExchangePrivate; }

  CurrencyExchangeFlatSet queryTradableCurrencies() {
    return hasPrivateAPI() ? _pExchangePrivate->queryTradableCurrencies() : _exchangePublic.queryTradableCurrencies();
  }

  CurrencyExchange convertStdCurrencyToCurrencyExchange(CurrencyCode currencyCode) {
    return _exchangePublic.convertStdCurrencyToCurrencyExchange(currencyCode);
  }

  MarketSet queryTradableMarkets() { return _exchangePublic.queryTradableMarkets(); }

  MarketPriceMap queryAllPrices() { return _exchangePublic.queryAllPrices(); }

  WithdrawalFeeMap queryWithdrawalFees() {
    return hasPrivateAPI() ? _pExchangePrivate->queryWithdrawalFees() : _exchangePublic.queryWithdrawalFees();
  }

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode) {
    return hasPrivateAPI() ? _pExchangePrivate->queryWithdrawalFee(currencyCode)
                           : _exchangePublic.queryWithdrawalFee(currencyCode);
  }

  MarketOrderBookMap queryAllApproximatedOrderBooks(int depth = ExchangePublic::kDefaultDepth) {
    return _exchangePublic.queryAllApproximatedOrderBooks(depth);
  }

  MarketOrderBook queryOrderBook(Market m, int depth = ExchangePublic::kDefaultDepth) {
    return _exchangePublic.queryOrderBook(m, depth);
  }

  MonetaryAmount queryLast24hVolume(Market m) { return _exchangePublic.queryLast24hVolume(m); }

  /// Retrieve an ordered vector of recent last trades
  LastTradesVector queryLastTrades(Market m, int nbTrades = ExchangePublic::kNbLastTradesDefault) {
    return _exchangePublic.queryLastTrades(m, nbTrades);
  }

  /// Retrieve the last price of given market.
  MonetaryAmount queryLastPrice(Market m) { return _exchangePublic.queryLastPrice(m); }

  bool canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool canDeposit(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool matches(const ExchangeName &exchangeName) const {
    return name() == exchangeName.name() && (!exchangeName.isKeyNameDefined() || keyName() == exchangeName.keyName());
  }

  void updateCacheFile() const;

 private:
  api::ExchangePublic &_exchangePublic;
  api::ExchangePrivate *_pExchangePrivate = nullptr;
  const ExchangeInfo &_exchangeInfo;
};
}  // namespace cct
