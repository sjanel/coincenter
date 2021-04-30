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
  /// Builds a Exchange without private exchange. All private requests will be forbidden.
  Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic);

  /// Build a Exchange with both private and public exchanges
  Exchange(const ExchangeInfo &exchangeInfo, api::ExchangePublic &exchangePublic,
           api::ExchangePrivate &exchangePrivate);

  std::string_view name() const { return _exchangePublic.name(); }
  std::string_view keyName() const { return _exchangePrivate.keyName(); }

  api::ExchangePublic &apiPublic() { return _exchangePublic; }
  api::ExchangePrivate &apiPrivate() { return _exchangePrivate; }
  const ExchangeInfo &exchangeInfo() const { return _exchangeInfo; }

  bool hasPrivateAPI() const;

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
  api::ExchangePrivate &_exchangePrivate;
  const ExchangeInfo &_exchangeInfo;
};
}  // namespace cct