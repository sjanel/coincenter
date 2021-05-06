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
  static constexpr char kDepositAddressesFilename[] = ".depositaddresses";

  /// Flag controlling the behavior of the wallet creation security:
  ///  false: do not validate and read wallets from above file, only trust deposit address method from target exchange
  ///  true: always validate wallet at construction from the known ones defined in the above file. Exception will be
  ///        thrown in case addresses mismatched compared to the query of deposit addresses from target exchange
  static constexpr bool kValidateWalletFromDepositAddressesFile = true;

  /// Build an empty wallet, which cannot be used.
  Wallet() = default;

  /// Build a wallet with all information.
  /// Be careful: if such a wallet is not present in 'kDepositAddressesFilename'.json file,
  /// exception will be thrown if kValidateWalletFromDepositAddressesFile is true
  Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag);

  /// Build a wallet with all information.
  /// Be careful: if such a wallet is not present in 'kDepositAddressesFilename'.json file,
  /// exception will be thrown if kValidateWalletFromDepositAddressesFile is true
  Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
         std::string_view tag);

  const PrivateExchangeName &privateExchangeName() const { return _privateExchangeName; }

  std::string_view exchangeName() const { return _privateExchangeName.name(); }

  const std::string &address() const;

  const std::string &destinationTag() const { return _tag; }

  CurrencyCode currencyCode() const { return _currency; }

  bool empty() const { return _address.empty(); }

  bool isValid() const { return !_address.empty(); }

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
