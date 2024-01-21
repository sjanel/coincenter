#include "exchange.hpp"

#include <memory>

#include "cct_log.hpp"
#include "currencycode.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchangeinfo.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"

namespace cct {

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic,
                   api::ExchangePrivate &exchangePrivate)
    : _exchangePublic(exchangePublic),
      _pExchangePrivate(std::addressof(exchangePrivate)),
      _exchangeInfo(exchangeInfo) {}

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic)
    : _exchangePublic(exchangePublic), _exchangeInfo(exchangeInfo) {}

bool Exchange::canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  if (_exchangeInfo.excludedCurrenciesWithdrawal().contains(currencyCode)) {
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
  auto lb = currencyExchangeSet.find(currencyCode);
  if (lb == currencyExchangeSet.end()) {
    log::trace("{} cannot be deposited on {}", currencyCode, name());
    return false;
  }
  return lb->canDeposit();
}

void Exchange::updateCacheFile() const {
  _exchangePublic.updateCacheFile();
  if (_pExchangePrivate != nullptr) {
    _pExchangePrivate->updateCacheFile();
  }
}
}  // namespace cct
