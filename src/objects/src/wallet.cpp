#include "wallet.hpp"

#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "jsonhelpers.hpp"

namespace cct {
namespace {
/// Flag controlling the behavior of the wallet creation security:
///  false: do not validate and read wallets from above file, only trust deposit address method from target exchange
///  true: always validate wallet at construction from the known ones defined in the above file. Exception will be
///        thrown in case addresses mismatched compared to the query of deposit addresses from target exchange
static constexpr bool kValidateWalletFromDepositAddressesFile = true;
}  // namespace

bool Wallet::IsAddressPresentInDepositFile(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                                           std::string_view expectedAddress, std::string_view expectedTag) {
  json data = OpenJsonFile(Wallet::kDepositAddressesFilename, FileNotFoundMode::kThrow, FileType::kConfig);
  if (!data.contains(privateExchangeName.name())) {
    log::warn("No deposit addresses found in {} for {}", Wallet::kDepositAddressesFilename, privateExchangeName.name());
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
        if (expectedAddress != address) {
          return false;
        }
        std::string_view tag(tagPos == std::string::npos ? addressAndTag.end() : (addressAndTag.begin() + tagPos + 1),
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
    throw exception("Incorrect wallet compared to the one stored in " + std::string(Wallet::kDepositAddressesFilename));
  }
#endif
}

Wallet::Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag)
    : _privateExchangeName(std::move(privateExchangeName)), _address(address), _tag(tag), _currency(currency) {
#ifndef CCT_DO_NOT_VALIDATE_DEPOSIT_ADDRESS_IN_FILE
  if (!IsAddressPresentInDepositFile(_privateExchangeName, currency, address, tag)) {
    throw exception("Incorrect wallet compared to the one stored in " + std::string(Wallet::kDepositAddressesFilename));
  }
#endif
}

std::string_view Wallet::address() const {
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