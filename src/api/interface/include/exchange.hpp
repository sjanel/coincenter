#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "exchangeinfo.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapi.hpp"
#include "exchangepublicapi.hpp"

namespace cct {
class Exchange {
 public:
  using WithdrawalFeeMap = api::ExchangePublic::WithdrawalFeeMap;

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
    throw exception("Cannot use default private exchange");
  }

  const api::ExchangePrivate &apiPrivate() const {
    if (_pExchangePrivate) {
      return *_pExchangePrivate;
    }
    throw exception("Cannot use default private exchange");
  }

  const ExchangeInfo &exchangeInfo() const { return _exchangeInfo; }

  bool hasPrivateAPI() const { return _pExchangePrivate; }

  CurrencyExchangeFlatSet queryTradableCurrencies();

  WithdrawalFeeMap queryWithdrawalFees();

  MonetaryAmount queryWithdrawalFee(CurrencyCode currencyCode);

  bool canWithdraw(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool canDeposit(CurrencyCode currencyCode, const CurrencyExchangeFlatSet &currencyExchangeSet) const;

  bool matches(const PrivateExchangeName &privateExchangeName) const {
    return name() == privateExchangeName.name() && keyName() == privateExchangeName.keyName();
  }

  bool matchesKeyNameWildcard(const PrivateExchangeName &privateExchangeName) const {
    return name() == privateExchangeName.name() &&
           (!privateExchangeName.isKeyNameDefined() || keyName() == privateExchangeName.keyName());
  }

  void updateCacheFile() const;

 private:
  api::ExchangePublic &_exchangePublic;
  api::ExchangePrivate *_pExchangePrivate;
  const ExchangeInfo &_exchangeInfo;
};
}  // namespace cct
