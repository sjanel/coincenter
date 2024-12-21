#include "apikeysprovider.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "accountowner.hpp"
#include "apikey.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "exchangename.hpp"
#include "exchangesecretsinfo.hpp"
#include "file.hpp"
#include "read-json.hpp"
#include "runmodes.hpp"
#include "secret-schema.hpp"

namespace cct::api {
namespace {

std::string_view GetSecretFileName(settings::RunMode runMode) {
  if (settings::AreTestKeysRequested(runMode)) {
    log::info("Test mode activated, shifting to secret_test.json file.");
    return "secret_test.json";
  }
  return "secret.json";
}

}  // namespace

APIKeysProvider::APIKeysProvider(std::string_view dataDir, const ExchangeSecretsInfo& exchangeSecretsInfo,
                                 settings::RunMode runMode)
    : _apiKeysPerExchange(ParseAPIKeys(dataDir, exchangeSecretsInfo, runMode)) {
  if (log::get_level() <= log::level::debug) {
    string foundKeysStr = str();
    if (!foundKeysStr.empty()) {
      log::debug("Loaded keys {}", foundKeysStr);
    }
  }
}

APIKeysProvider::KeyNames APIKeysProvider::getKeyNames(ExchangeNameEnum exchangeNameEnum) const {
  KeyNames keyNames;

  const APIKeys& apiKeys = _apiKeysPerExchange[static_cast<int>(exchangeNameEnum)];
  std::ranges::transform(apiKeys, std::back_inserter(keyNames), [](const APIKey& apiKey) { return apiKey.name(); });

  return keyNames;
}

const APIKey& APIKeysProvider::get(const ExchangeName& exchangeName) const {
  ExchangeNameEnum exchangeNameEnum = exchangeName.exchangeNameEnum();
  const APIKeys& apiKeys = _apiKeysPerExchange[static_cast<int>(exchangeNameEnum)];
  if (!exchangeName.isKeyNameDefined()) {
    if (apiKeys.size() > 1) {
      throw exception("Specify name for {} keys as you have several", exchangeName.name());
    }
    return apiKeys.front();
  }
  auto keyNameIt = std::ranges::find_if(
      apiKeys, [exchangeName](const APIKey& apiKey) { return apiKey.name() == exchangeName.keyName(); });
  if (keyNameIt == apiKeys.end()) {
    throw exception("Unable to retrieve private key for {} named {:k}", exchangeName.name(), exchangeName);
  }
  return *keyNameIt;
}

APIKeysProvider::APIKeysPerExchange APIKeysProvider::ParseAPIKeys(std::string_view dataDir,
                                                                  const ExchangeSecretsInfo& exchangeSecretsInfo,
                                                                  settings::RunMode runMode) {
  APIKeysProvider::APIKeysPerExchange apiKeysPerExchange;

  if (exchangeSecretsInfo.allExchangesWithoutSecrets()) {
    log::info("Not loading private keys, using only public exchanges");
    return apiKeysPerExchange;
  }

  std::string_view secretFileName = GetSecretFileName(runMode);
  const auto throwOrNoThrow = settings::AreTestKeysRequested(runMode) ? File::IfError::kThrow : File::IfError::kNoThrow;
  File secretsFile(dataDir, File::Type::kSecret, secretFileName, throwOrNoThrow);

  schema::APIKeysPerExchangeMap apiKeysPerExchangeMap;

  ReadExactJsonOrThrow(secretsFile.readAll(), apiKeysPerExchangeMap);

  const auto& exchangesWithoutSecrets = exchangeSecretsInfo.exchangesWithoutSecrets();

  bool atLeastOneKeyFound = false;
  for (auto& [exchangeNameEnum, apiKeys] : apiKeysPerExchangeMap) {
    if (std::ranges::any_of(exchangesWithoutSecrets, [exchangeNameEnum](const auto& exchangeName) {
          return exchangeName.exchangeNameEnum() == exchangeNameEnum;
        })) {
      log::debug("Not loading {} private keys as requested", EnumToString(exchangeNameEnum));
      continue;
    }

    for (auto& [keyName, apiKey] : apiKeys) {
      if (apiKey.key.empty() || apiKey.priv.empty()) {
        log::error("Wrong format for secret.json file. It should contain at least fields 'key' and 'private'");
        continue;
      }

      apiKeysPerExchange[static_cast<int>(exchangeNameEnum)].emplace_back(
          keyName, std::move(apiKey.key), std::move(apiKey.priv), std::move(apiKey.passphrase),
          AccountOwner(apiKey.accountOwner.enName, apiKey.accountOwner.koName));

      atLeastOneKeyFound = true;
    }
  }
  if (!atLeastOneKeyFound) {
    log::warn("No private api keys file '{}' found. Only public exchange queries will be supported", secretFileName);
  }

  return apiKeysPerExchange;
}

string APIKeysProvider::str() const {
  string foundKeysStr;
  for (int exchangePos = 0; exchangePos < kNbSupportedExchanges; ++exchangePos) {
    const APIKeys& apiKeys = _apiKeysPerExchange[exchangePos];
    if (!foundKeysStr.empty()) {
      foundKeysStr.append(" | ");
    }
    foundKeysStr.push_back('{');
    bool firstKey = true;
    for (const APIKey& key : apiKeys) {
      if (!firstKey) {
        foundKeysStr.push_back(',');
      }
      foundKeysStr.append(key.name());
      firstKey = false;
    }
    foundKeysStr.push_back('}');
    foundKeysStr.push_back('@');
    foundKeysStr.append(EnumToString(static_cast<ExchangeNameEnum>(exchangePos)));
  }
  return foundKeysStr;
}

}  // namespace cct::api