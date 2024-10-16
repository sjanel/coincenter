#include "wallet.hpp"

#include <algorithm>
#include <string_view>
#include <utility>

#include "accountowner.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "deposit-addresses.hpp"
#include "exchangename.hpp"

namespace cct {

/// Test existence of deposit address (and optional tag) in the trusted deposit addresses file.
bool Wallet::ValidateWallet(WalletCheck walletCheck, const ExchangeName &exchangeName, CurrencyCode currency,
                            std::string_view expectedAddress, std::string_view expectedTag) {
  if (!walletCheck.doCheck()) {
    log::debug("No wallet validation from file, consider OK");
    return true;
  }
  auto depositAddresses = ReadDepositAddresses(walletCheck.dataDir());
  auto exchangeNameIt = depositAddresses.find(exchangeName.name());
  if (exchangeNameIt == depositAddresses.end()) {
    log::warn("No deposit addresses found in {} for {}", kDepositAddressesFileName, exchangeName);
    return false;
  }
  const auto &exchangeDepositAddresses = exchangeNameIt->second;
  bool uniqueKeyName = true;
  for (const auto &[privateExchangeKeyName, accountDepositAddresses] : exchangeDepositAddresses) {
    if (exchangeName.keyName().empty()) {
      if (!uniqueKeyName) {
        log::error("Several key names found for exchange {:n}. Specify a key name to remove ambiguity", exchangeName);
        return false;
      }

      uniqueKeyName = false;
    } else if (exchangeName.keyName() != privateExchangeKeyName) {
      continue;
    }
    for (const auto &[currencyCode, addressAndTag] : accountDepositAddresses) {
      if (currencyCode == currency) {
        auto tagPos = addressAndTag.find(',');
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
               WalletCheck walletCheck, AccountOwner accountOwner)
    : _exchangeName(std::move(exchangeName)),
      _addressAndTag(std::move(address)),
      _accountOwner(std::move(accountOwner)),
      _tagPos(tag.empty() ? std::string_view::npos : _addressAndTag.size()),
      _currency(currency) {
  _addressAndTag.append(tag);
  if (!ValidateWallet(walletCheck, _exchangeName, _currency, this->address(), tag)) {
    throw exception("Incorrect wallet compared to the one stored in {}", kDepositAddressesFileName);
  }
}

}  // namespace cct