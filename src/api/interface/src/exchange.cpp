#include "exchange.hpp"

#include <memory>

#include "cct_log.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeconfig.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "marketorderbook.hpp"
#include "public-trade-vector.hpp"

namespace cct {

Exchange::Exchange(const ExchangeConfig &exchangeConfig, api::ExchangePublic &exchangePublic,
                   api::ExchangePrivate &exchangePrivate)
    : _exchangePublic(exchangePublic),
      _pExchangePrivate(std::addressof(exchangePrivate)),
      _exchangeConfig(exchangeConfig) {}

Exchange::Exchange(const ExchangeConfig &exchangeConfig, api::ExchangePublic &exchangePublic)
    : _exchangePublic(exchangePublic), _exchangeConfig(exchangeConfig) {}

bool Exchange::canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  if (_exchangeConfig.excludedCurrenciesWithdrawal().contains(currencyCode)) {
    return false;
  }
  auto lb = currencyExchangeSet.find(currencyCode);
  if (lb == currencyExchangeSet.end()) {
    log::trace("{} cannot be withdrawn from {}", currencyCode, name());
    return false;
  }
  return lb->canWithdraw();
}

bool Exchange::canDeposit(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  const auto lb = currencyExchangeSet.find(currencyCode);
  if (lb == currencyExchangeSet.end()) {
    log::trace("{} cannot be deposited on {}", currencyCode, name());
    return false;
  }
  return lb->canDeposit();
}

MarketOrderBook Exchange::queryOrderBook(Market mk, int depth) { return _exchangePublic.queryOrderBook(mk, depth); }

/// Retrieve an ordered vector of recent last trades
PublicTradeVector Exchange::queryLastTrades(Market mk, int nbTrades) {
  return _exchangePublic.queryLastTrades(mk, nbTrades);
}

void Exchange::updateCacheFile() const {
  _exchangePublic.updateCacheFile();
  if (_pExchangePrivate != nullptr) {
    _pExchangePrivate->updateCacheFile();
  }
}
}  // namespace cct
