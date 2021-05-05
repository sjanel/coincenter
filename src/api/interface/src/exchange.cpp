#include "exchange.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "exchangeprivatedefaultapi.hpp"

namespace cct {
namespace {

api::ExchangePrivateDefault gExchangePrivateDefault;
}  // namespace

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic,
                   api::ExchangePrivate &exchangePrivate)
    : _exchangePublic(exchangePublic), _exchangePrivate(exchangePrivate), _exchangeInfo(exchangeInfo) {}

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic)
    : Exchange(exchangeInfo, exchangePublic, gExchangePrivateDefault) {}

bool Exchange::hasPrivateAPI() const {
  return std::addressof(gExchangePrivateDefault) != std::addressof(_exchangePrivate);
}

CurrencyExchangeFlatSet Exchange::queryTradableCurrencies() {
  return hasPrivateAPI() ? _exchangePrivate.queryTradableCurrencies() : _exchangePublic.queryTradableCurrencies();
}

bool Exchange::canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  if (_exchangeInfo.excludedCurrenciesWithdrawal().contains(currencyCode)) {
    return false;
  }
  auto lb = currencyExchangeSet.find(currencyCode);
  if (lb == currencyExchangeSet.end()) {
    log::trace("{} cannot be withdrawed from {}", currencyCode.str(), name());
    return false;
  }
  return lb->canWithdraw();
}

bool Exchange::canDeposit(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const {
  auto lb = currencyExchangeSet.find(currencyCode);
  if (lb == currencyExchangeSet.end()) {
    log::trace("{} cannot be deposited on {}", currencyCode.str(), name());
    return false;
  }
  return lb->canDeposit();
}

void Exchange::updateCacheFile() const {
  _exchangePublic.updateCacheFile();
  _exchangePrivate.updateCacheFile();
}
}  // namespace cct