#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"

namespace cct {
class CoincenterInfo;
class Wallet {
 public:
  /// Build a wallet with all information.
  Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag, const CoincenterInfo &coincenterInfo);

  Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag, const CoincenterInfo &coincenterInfo);

  const PrivateExchangeName &privateExchangeName() const { return _privateExchangeName; }

  std::string_view exchangeName() const { return _privateExchangeName.name(); }

  std::string_view address() const { return _address; }

  std::string_view destinationTag() const { return _tag; }

  CurrencyCode currencyCode() const { return _currency; }

  bool hasDestinationTag() const { return !_tag.empty(); }

  string str() const;

  bool operator==(const Wallet &w) const {
    return _currency == w._currency && _privateExchangeName == w._privateExchangeName && _address == w._address &&
           _tag == w._tag;
  }
  bool operator!=(const Wallet &w) const { return !(*this == w); }

  static bool IsAddressPresentInDepositFile(std::string_view dataDir, const PrivateExchangeName &privateExchangeName,
                                            CurrencyCode currency, std::string_view expectedAddress,
                                            std::string_view expectedTag);

  using trivially_relocatable = std::integral_constant<bool, is_trivially_relocatable_v<PrivateExchangeName> &&
                                                                 is_trivially_relocatable_v<string> >::type;

 private:
  PrivateExchangeName _privateExchangeName;
  string _address;
  string _tag;
  CurrencyCode _currency;
};

}  // namespace cct
