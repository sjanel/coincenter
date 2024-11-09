#include "exchange.hpp"

#include <cstddef>
#include <memory>
#include <utility>

#include "cct_log.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"
#include "marketorderbook.hpp"
#include "public-trade-vector.hpp"

namespace cct {

Exchange::Exchange(const schema::ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic)
    : Exchange(exchangeConfig, exchangePublic, std::unique_ptr<ExchangePrivate>()) {}

Exchange::Exchange(const schema::ExchangeConfig &exchangeConfig, ExchangePublic &exchangePublic,
                   std::unique_ptr<ExchangePrivate> exchangePrivate)
    : _pExchangePublic(std::addressof(exchangePublic)),
      _exchangePrivate(std::move(exchangePrivate)),
      _pExchangeConfig(std::addressof(exchangeConfig)) {}

std::size_t Exchange::publicExchangePos() const { return PublicExchangePos(name()); }

bool Exchange::canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  if (_pExchangeConfig->asset.withdrawExclude.contains(currencyCode)) {
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

MarketOrderBook Exchange::getOrderBook(Market mk, int depth) { return apiPublic().getOrderBook(mk, depth); }

/// Retrieve an ordered vector of recent last trades
PublicTradeVector Exchange::getLastTrades(Market mk, int nbTrades) { return apiPublic().getLastTrades(mk, nbTrades); }

void Exchange::updateCacheFile() const {
  apiPublic().updateCacheFile();
  if (_exchangePrivate) {
    _exchangePrivate->updateCacheFile();
  }
}
}  // namespace cct
