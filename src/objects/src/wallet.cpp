#include "wallet.hpp"

#include "cct_allfiles.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"

namespace cct {

/// Test existence of deposit address (and optional tag) in the trusted deposit addresses file.
bool Wallet::IsAddressPresentInDepositFile(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                                           std::string_view expectedAddress, std::string_view expectedTag) {
  json data = kDepositAddresses.readJson();
  if (!data.contains(privateExchangeName.name())) {
    log::warn("No deposit addresses found in {} for {}", kDepositAddresses.name(), privateExchangeName.name());
    return false;
  }
  const json &exchangeWallets = data[string(privateExchangeName.name())];
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
        string addressAndTag = value;
        std::size_t tagPos = addressAndTag.find_first_of(',');
        std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
        if (expectedAddress != address) {
          return false;
        }
        std::string_view tag(tagPos == string::npos ? addressAndTag.end() : (addressAndTag.begin() + tagPos + 1),
                             addressAndTag.end());
        if (expectedTag != tag) {
          return false;
        }
        return true;
      }
    }
  }

  log::error("Unknown currency {} for wallet", currency.str());
  return false;
}

Wallet::Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag)
    : _privateExchangeName(privateExchangeName), _address(address), _tag(tag), _currency(currency) {
#ifndef CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE
  if (!IsAddressPresentInDepositFile(_privateExchangeName, currency, address, tag)) {
    throw exception("Incorrect wallet compared to the one stored in " + string(kDepositAddresses.name()));
  }
#endif
}

Wallet::Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag)
    : _privateExchangeName(std::move(privateExchangeName)), _address(address), _tag(tag), _currency(currency) {
#ifndef CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE
  if (!IsAddressPresentInDepositFile(_privateExchangeName, currency, address, tag)) {
    throw exception("Incorrect wallet compared to the one stored in " + string(kDepositAddresses.name()));
  }
#endif
}

string Wallet::str() const {
  string ret(_privateExchangeName.str());
  ret.append(" wallet of ");
  ret.append(_currency.str());
  ret.append(", address: [");
  ret.append(_address);
  ret.push_back(']');
  if (!_tag.empty()) {
    ret.append(" tag ");
    ret.append(_tag);
  }
  return ret;
}
}  // namespace cct