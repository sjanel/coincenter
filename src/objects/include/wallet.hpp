#pragma once

#include <string>
#include <string_view>

#include "currencycode.hpp"
#include "exchangename.hpp"

namespace cct {

class Wallet {
 public:
  /// File containing all validated external addresses.
  /// It should be a json file with this format:
  /// {
  ///   "exchangeName1": {"BTC": "btcAddress", "XRP": "xrpAdress,xrpTag", "EOS": "eosAddress,eosTag"},
  ///   "exchangeName2": {...}
  /// }
  /// In case crypto contains an additional "tag", "memo" or other, it will be placed after the ',' in the address
  /// field.
  static constexpr char kDepositAddressesFilename[] = ".depositaddresses.json";

  /// Build a wallet with all information.
  /// Wallet will be validated against the trusted deposit addresses stored in deposit address files,
  /// unless CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE is set (controlled at build time, or for unit tests which do
  /// not withdraw)
  Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag);

  /// Build a wallet with all information.
  /// Wallet will be validated against the trusted deposit addresses stored in deposit address files,
  /// unless CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE is set (controlled at build time, or for unit tests which do
  /// not withdraw)
  Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag);

  const PrivateExchangeName &privateExchangeName() const { return _privateExchangeName; }

  std::string_view exchangeName() const { return _privateExchangeName.name(); }

  std::string_view address() const { return _address; }

  std::string_view destinationTag() const { return _tag; }

  CurrencyCode currencyCode() const { return _currency; }

  bool hasDestinationTag() const { return !_tag.empty(); }

  std::string str() const;

  bool operator==(const Wallet &w) const {
    return _currency == w._currency && _privateExchangeName == w._privateExchangeName && _address == w._address &&
           _tag == w._tag;
  }
  bool operator!=(const Wallet &w) const { return !(*this == w); }

  static bool IsAddressPresentInDepositFile(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                                            std::string_view expectedAddress, std::string_view expectedTag);

 private:
  PrivateExchangeName _privateExchangeName;
  std::string _address;
  std::string _tag;
  CurrencyCode _currency;
};

}  // namespace cct
