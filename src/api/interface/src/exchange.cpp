#include "exchange.hpp"

#include <memory>

#include "cct_log.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeconfig.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"

namespace cct {

Exchange::Exchange(const ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic)
    : Exchange(exchangeConfig, exchangePublic, std::unique_ptr<ExchangePrivate>()) {}

Exchange::Exchange(const ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic,
                   std::unique_ptr<ExchangePrivate> exchangePrivate)
    : _pExchangePublic(std::addressof(exchangePublic)),
      _exchangePrivate(std::move(exchangePrivate)),
      _pExchangeConfig(std::addressof(exchangeConfig)) {}

std::size_t Exchange::publicExchangePos() const { return PublicExchangePos(name()); }

bool Exchange::canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  if (_pExchangeConfig->excludedCurrenciesWithdrawal().contains(currencyCode)) {
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

MarketOrderBook Exchange::queryOrderBook(Market mk, int depth) { return apiPublic().queryOrderBook(mk, depth); }

/// Retrieve an ordered vector of recent last trades
PublicTradeVector Exchange::queryLastTrades(Market mk, int nbTrades) {
  return apiPublic().queryLastTrades(mk, nbTrades);
}

void Exchange::updateCacheFile() const {
  apiPublic().updateCacheFile();
  if (_exchangePrivate) {
    _exchangePrivate->updateCacheFile();
  }
}
}  // namespace cct
