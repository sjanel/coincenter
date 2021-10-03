#include "exchange.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"

namespace cct {

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic,
                   api::ExchangePrivate &exchangePrivate)
    : _exchangePublic(exchangePublic),
      _pExchangePrivate(std::addressof(exchangePrivate)),
      _exchangeInfo(exchangeInfo) {}

Exchange::Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic)
    : _exchangePublic(exchangePublic), _exchangeInfo(exchangeInfo) {}

CurrencyExchangeFlatSet Exchange::queryTradableCurrencies() {
  return hasPrivateAPI() ? _pExchangePrivate->queryTradableCurrencies() : _exchangePublic.queryTradableCurrencies();
}

Exchange::WithdrawalFeeMap Exchange::queryWithdrawalFees() {
  return hasPrivateAPI() ? _pExchangePrivate->queryWithdrawalFees() : _exchangePublic.queryWithdrawalFees();
}

MonetaryAmount Exchange::queryWithdrawalFee(CurrencyCode currencyCode) {
  return hasPrivateAPI() ? _pExchangePrivate->queryWithdrawalFee(currencyCode)
                         : _exchangePublic.queryWithdrawalFee(currencyCode);
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
  if (_pExchangePrivate) {
    _pExchangePrivate->updateCacheFile();
  }
}
}  // namespace cct