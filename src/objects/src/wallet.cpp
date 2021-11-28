#include "wallet.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_log.hpp"

namespace cct {

namespace {
File GetDepositAddressesFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kSecret, kDepositAddressesFileName, File::IfNotFound::kNoThrow);
}
}  // namespace
/// Test existence of deposit address (and optional tag) in the trusted deposit addresses file.
bool Wallet::ValidateWallet(WalletCheck walletCheck, const PrivateExchangeName &privateExchangeName,
                            CurrencyCode currency, std::string_view expectedAddress, std::string_view expectedTag) {
  if (!walletCheck.doCheck()) {
    log::debug("No wallet validation from file, consider OK");
    return true;
  }
  File depositAddressesFile = GetDepositAddressesFile(walletCheck.dataDir());
  json data = depositAddressesFile.readJson();
  if (!data.contains(privateExchangeName.name())) {
    log::warn("No deposit addresses found in {} for {}", kDepositAddressesFileName, privateExchangeName.name());
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
        std::size_t tagPos = addressAndTag.find(',');
        std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
        if (expectedAddress != address) {
          return false;
        }
        std::string_view tag(tagPos == string::npos ? addressAndTag.end() : (addressAndTag.begin() + tagPos + 1),
                             addressAndTag.end());
        return expectedTag == tag;
      }
    }
  }

  log::error("Unknown currency {} for wallet", currency.str());
  return false;
}

namespace {
void ValidateDepositAddressIfNeeded(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                                    std::string_view address, std::string_view tag, WalletCheck walletCheck) {
  if (!Wallet::ValidateWallet(walletCheck, privateExchangeName, currency, address, tag)) {
    string errMsg("Incorrect wallet compared to the one stored in ");
    errMsg.append(kDepositAddressesFileName);
    throw exception(std::move(errMsg));
  }
}
}  // namespace

Wallet::Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag, WalletCheck walletCheck)
    : _privateExchangeName(privateExchangeName),
      _addressAndTag(address),
      _tagPos(tag.empty() ? string::npos : address.size()),
      _currency(currency) {
  _addressAndTag.append(tag);
  ValidateDepositAddressIfNeeded(_privateExchangeName, currency, address, tag, walletCheck);
}

Wallet::Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, string &&address, std::string_view tag,
               WalletCheck walletCheck)
    : _privateExchangeName(std::move(privateExchangeName)),
      _addressAndTag(std::move(address)),
      _tagPos(tag.empty() ? string::npos : _addressAndTag.size()),
      _currency(currency) {
  _addressAndTag.append(tag);
  ValidateDepositAddressIfNeeded(_privateExchangeName, currency, this->address(), tag, walletCheck);
}

string Wallet::str() const {
  string ret(_privateExchangeName.str());
  ret.append(" wallet of ");
  ret.append(_currency.str());
  ret.append(", address: [");
  ret.append(address());
  ret.push_back(']');
  if (hasTag()) {
    ret.append(" tag ");
    ret.append(tag());
  }
  return ret;
}
}  // namespace cct