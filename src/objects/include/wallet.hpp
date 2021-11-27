#pragma once

#include <string_view>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"

namespace cct {
class CoincenterInfo;
class Wallet {
 public:
  /// Empty wallet that should not be used, just for temporary purposes.
  Wallet() noexcept = default;

  /// Build a wallet with all information.
  Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag, const CoincenterInfo &coincenterInfo);

  Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, string &&address, string &&tag,
         const CoincenterInfo &coincenterInfo);

  const PrivateExchangeName &privateExchangeName() const { return _privateExchangeName; }

  std::string_view exchangeName() const { return _privateExchangeName.name(); }

  std::string_view address() const {
    check();
    return _address;
  }

  std::string_view destinationTag() const {
    check();
    return _tag;
  }

  CurrencyCode currencyCode() const { return _currency; }

  bool hasDestinationTag() const { return !destinationTag().empty(); }

  string str() const;

  bool operator==(const Wallet &w) const {
    return _currency == w._currency && _privateExchangeName == w._privateExchangeName && _address == w._address &&
           _tag == w._tag;
  }
  bool operator!=(const Wallet &w) const { return !(*this == w); }

  static bool ValidateWallet(std::string_view dataDir, const PrivateExchangeName &privateExchangeName,
                             CurrencyCode currency, std::string_view expectedAddress, std::string_view expectedTag);

  using trivially_relocatable = std::integral_constant<bool, is_trivially_relocatable_v<PrivateExchangeName> &&
                                                                 is_trivially_relocatable_v<string> >::type;

 private:
  inline void check() const {
    if (CCT_UNLIKELY(_address.empty())) {
      throw exception("Cannot use an empty wallet address!");
    }
  }

  PrivateExchangeName _privateExchangeName;
  string _address;
  string _tag;
  CurrencyCode _currency;
};

}  // namespace cct
