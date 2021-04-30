#include "wallet.hpp"

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace {

bool ValidateWalletFromFile(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                            std::string_view expectedAddress = "", std::string_view expectedTag = "") {
  json data = OpenJsonFile(Wallet::kDepositAddressesFilename, FileNotFoundMode::kThrow);
  if (!data.contains(privateExchangeName.name())) {
    log::warn("Unknown exchange {} for wallet", privateExchangeName.name());
    return false;
  }
  const json &exchangeWallets = data[std::string(privateExchangeName.name())];
  bool uniqueKeyName = true;
  for (const auto &[privateExchangeKeyName, wallets] : exchangeWallets.items()) {
    if (privateExchangeName.keyName().empty()) {
      if (!uniqueKeyName) {
        log::error("Several key names found for exchange {}. Specify a key name to remove ambiguity",
                   privateExchangeName.name());
        return false;
      }

      uniqueKeyName = false;
    } else if (privateExchangeName.keyName() != privateExchangeKeyName) {
      continue;
    }
    for (const auto &[currencyCodeStr, value] : wallets.items()) {
      CurrencyCode currencyCode(currencyCodeStr);
      if (currencyCode == currency) {
        std::string addressAndTag = value;
        std::size_t tagPos = addressAndTag.find_first_of(',');
        std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
        std::string_view tag(tagPos == std::string::npos ? addressAndTag.end() : (addressAndTag.begin() + tagPos + 1),
                             addressAndTag.end());
        if (!expectedAddress.empty()) {
          if (expectedAddress != address) {
            throw exception("Invalid wallet because " + std::string(expectedAddress) + " unknown from " +
                            Wallet::kDepositAddressesFilename);
          }
          if (expectedTag != tag) {
            throw exception("Invalid wallet because " + std::string(expectedTag) + " unknown from " +
                            Wallet::kDepositAddressesFilename);
          }
        }
        return true;
      }
    }
  }

  log::error("Unknown currency {} for wallet", currency.str());
  return false;
}
}  // namespace

Wallet::Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag)
    : _privateExchangeName(privateExchangeName), _address(address), _tag(tag), _currency(currency) {
  if (kValidateWalletFromDepositAddressesFile &&
      !ValidateWalletFromFile(_privateExchangeName, currency, address, tag)) {
    throw exception("Incorrect wallet compared to the one stored in " + std::string(Wallet::kDepositAddressesFilename));
  }
}

Wallet::Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag)
    : _privateExchangeName(std::move(privateExchangeName)), _address(address), _tag(tag), _currency(currency) {
  if (kValidateWalletFromDepositAddressesFile &&
      !ValidateWalletFromFile(_privateExchangeName, currency, address, tag)) {
    throw exception("Incorrect wallet compared to the one stored in " + std::string(Wallet::kDepositAddressesFilename));
  }
}

const std::string &Wallet::address() const {
  if (!isValid()) {
    // Security to avoid risk of withdrawing to invalid address
    throw exception("Cannot use invalid address!");
  }
  return _address;
}

std::string Wallet::str() const {
  std::string ret(_privateExchangeName.str());
  ret.append(" wallet of ");
  ret.append(_currency.str());
  ret.append(", address: [");
  if (isValid()) {
    ret.append(_address);
  } else {
    ret.append("INVALID");
  }
  ret.push_back(']');
  if (!_tag.empty()) {
    ret.append(" tag ");
    ret.append(_tag);
  }
  return ret;
}
}  // namespace cct