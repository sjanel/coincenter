#include "wallet.hpp"

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_log.hpp"
#include "coincenterinfo.hpp"

namespace cct {

namespace {
File GetDepositAddressesFile(std::string_view dataDir) {
  return File(dataDir, File::Type::kSecret, kDepositAddressesFileName, File::IfNotFound::kNoThrow);
}
}  // namespace
/// Test existence of deposit address (and optional tag) in the trusted deposit addresses file.
bool Wallet::ValidateWallet(std::string_view dataDir, const PrivateExchangeName &privateExchangeName,
                            CurrencyCode currency, std::string_view expectedAddress, std::string_view expectedTag) {
  File depositAddressesFile = GetDepositAddressesFile(dataDir);
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

namespace {
void ValidateDepositAddressIfNeeded(const PrivateExchangeName &privateExchangeName, CurrencyCode currency,
                                    std::string_view address, std::string_view tag,
                                    const CoincenterInfo &coincenterInfo) {
  if (coincenterInfo.exchangeInfo(privateExchangeName.name()).validateDepositAddressesInFile() &&
      !Wallet::ValidateWallet(coincenterInfo.dataDir(), privateExchangeName, currency, address, tag)) {
    string errMsg("Incorrect wallet compared to the one stored in ");
    errMsg.append(kDepositAddressesFileName);
    throw exception(std::move(errMsg));
  }
}
}  // namespace

Wallet::Wallet(const PrivateExchangeName &privateExchangeName, CurrencyCode currency, std::string_view address,
               std::string_view tag, const CoincenterInfo &coincenterInfo)
    : _privateExchangeName(privateExchangeName), _address(address), _tag(tag), _currency(currency) {
  ValidateDepositAddressIfNeeded(_privateExchangeName, currency, address, tag, coincenterInfo);
}

Wallet::Wallet(PrivateExchangeName &&privateExchangeName, CurrencyCode currency, string &&address, string &&tag,
               const CoincenterInfo &coincenterInfo)
    : _privateExchangeName(std::move(privateExchangeName)),
      _address(std::move(address)),
      _tag(std::move(tag)),
      _currency(currency) {
  ValidateDepositAddressIfNeeded(_privateExchangeName, currency, _address, _tag, coincenterInfo);
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