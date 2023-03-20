#include "wallet.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_log.hpp"

namespace cct {

namespace {
File GetDepositAddressesFile(std::string_view dataDir) {
  return {dataDir, File::Type::kSecret, kDepositAddressesFileName, File::IfError::kNoThrow};
}
}  // namespace
/// Test existence of deposit address (and optional tag) in the trusted deposit addresses file.
bool Wallet::ValidateWallet(WalletCheck walletCheck, const ExchangeName &exchangeName, CurrencyCode currency,
                            std::string_view expectedAddress, std::string_view expectedTag) {
  if (!walletCheck.doCheck()) {
    log::debug("No wallet validation from file, consider OK");
    return true;
  }
  File depositAddressesFile = GetDepositAddressesFile(walletCheck.dataDir());
  json data = depositAddressesFile.readJson();
  if (!data.contains(exchangeName.name())) {
    log::warn("No deposit addresses found in {} for {}", kDepositAddressesFileName, exchangeName);
    return false;
  }
  const json &exchangeWallets = data[string(exchangeName.name())];
  bool uniqueKeyName = true;
  for (const auto &[privateExchangeKeyName, wallets] : exchangeWallets.items()) {
    if (exchangeName.keyName().empty()) {
      if (!uniqueKeyName) {
        log::error("Several key names found for exchange {:n}. Specify a key name to remove ambiguity", exchangeName);
        return false;
      }

      uniqueKeyName = false;
    } else if (exchangeName.keyName() != privateExchangeKeyName) {
      continue;
    }
    for (const auto &[currencyCodeStr, value] : wallets.items()) {
      CurrencyCode currencyCode(currencyCodeStr);
      if (currencyCode == currency) {
        std::string_view addressAndTag = value.get<std::string_view>();
        std::size_t tagPos = addressAndTag.find(',');
        std::string_view address(addressAndTag.begin(), addressAndTag.begin() + std::min(tagPos, addressAndTag.size()));
        if (expectedAddress != address) {
          return false;
        }
        std::string_view tag(
            tagPos == std::string_view::npos ? addressAndTag.end() : (addressAndTag.begin() + tagPos + 1),
            addressAndTag.end());
        return expectedTag == tag;
      }
    }
  }

  log::error("Unknown currency {} for wallet", currency);
  return false;
}

Wallet::Wallet(ExchangeName exchangeName, CurrencyCode currency, string address, std::string_view tag,
               WalletCheck walletCheck)
    : _exchangeName(std::move(exchangeName)),
      _addressAndTag(std::move(address)),
      _tagPos(tag.empty() ? std::string_view::npos : _addressAndTag.size()),
      _currency(currency) {
  _addressAndTag.append(tag);
  if (!ValidateWallet(walletCheck, _exchangeName, _currency, this->address(), tag)) {
    throw exception("Incorrect wallet compared to the one stored in {}", kDepositAddressesFileName);
  }
}

}  // namespace cct