#include "apikeysprovider.hpp"

#include <algorithm>
#include <iterator>
#include <string_view>
#include <utility>

#include "accountowner.hpp"
#include "apikey.hpp"
#include "cct_exception.hpp"
#include "cct_json-container.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"
#include "exchangename.hpp"
#include "exchangesecretsinfo.hpp"
#include "file.hpp"
#include "runmodes.hpp"

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
    : _apiKeysMap(ParseAPIKeys(dataDir, exchangeSecretsInfo, runMode)) {
  if (log::get_level() <= log::level::debug) {
    string foundKeysStr = str();
    if (!foundKeysStr.empty()) {
      log::debug("Loaded keys {}", foundKeysStr);
    }
  }
}

APIKeysProvider::KeyNames APIKeysProvider::getKeyNames(std::string_view platform) const {
  KeyNames keyNames;
  auto foundIt = _apiKeysMap.find(platform);
  if (foundIt != _apiKeysMap.end()) {
    const APIKeys& apiKeys = foundIt->second;
    std::ranges::transform(apiKeys, std::back_inserter(keyNames), [](const APIKey& apiKey) { return apiKey.name(); });
  }
  return keyNames;
}

const APIKey& APIKeysProvider::get(const ExchangeName& exchangeName) const {
  std::string_view platformStr = exchangeName.name();
  auto foundIt = _apiKeysMap.find(platformStr);
  if (foundIt == _apiKeysMap.end()) {
    throw exception("Unable to retrieve private key for {}", platformStr);
  }
  const APIKeys& apiKeys = foundIt->second;
  if (!exchangeName.isKeyNameDefined()) {
    if (apiKeys.size() > 1) {
      throw exception("Specify name for {} keys as you have several", platformStr);
    }
    return apiKeys.front();
  }
  auto keyNameIt = std::ranges::find_if(
      apiKeys, [exchangeName](const APIKey& apiKey) { return apiKey.name() == exchangeName.keyName(); });
  if (keyNameIt == apiKeys.end()) {
    throw exception("Unable to retrieve private key for {} named {:k}", platformStr, exchangeName);
  }
  return *keyNameIt;
}

APIKeysProvider::APIKeysMap APIKeysProvider::ParseAPIKeys(std::string_view dataDir,
                                                          const ExchangeSecretsInfo& exchangeSecretsInfo,
                                                          settings::RunMode runMode) {
  APIKeysProvider::APIKeysMap map;
  if (exchangeSecretsInfo.allExchangesWithoutSecrets()) {
    log::info("Not loading private keys, using only public exchanges");
  } else {
    std::string_view secretFileName = GetSecretFileName(runMode);
    File secretsFile(dataDir, File::Type::kSecret, secretFileName,
                     settings::AreTestKeysRequested(runMode) ? File::IfError::kThrow : File::IfError::kNoThrow);
    json::container jsonData = secretsFile.readAllJson();
    for (auto& [publicExchangeName, keyObj] : jsonData.items()) {
      const auto& exchangesWithoutSecrets = exchangeSecretsInfo.exchangesWithoutSecrets();
      if (std::ranges::find(exchangesWithoutSecrets, ExchangeName(publicExchangeName)) !=
          exchangesWithoutSecrets.end()) {
        log::info("Not loading {} private keys as requested", publicExchangeName);
        continue;
      }
      for (auto& [name, keySecretObj] : keyObj.items()) {
        auto keyIt = keySecretObj.find("key");
        auto privateIt = keySecretObj.find("private");
        if (keyIt == keySecretObj.end() || privateIt == keySecretObj.end()) {
          log::error("Wrong format for secret.json file. It should contain at least fields 'key' and 'private'");
          continue;
        }
        string passphrase;
        auto passphraseIt = keySecretObj.find("passphrase");
        if (passphraseIt != keySecretObj.end()) {
          passphrase = std::move(passphraseIt->get_ref<string&>());
        }
        std::string_view ownerEnName;
        std::string_view ownerKoName;
        auto accountOwnerPartIt = keySecretObj.find("accountOwner");
        if (accountOwnerPartIt != keySecretObj.end()) {
          auto ownerEnNameIt = accountOwnerPartIt->find("enName");
          if (ownerEnNameIt != accountOwnerPartIt->end()) {
            ownerEnName = ownerEnNameIt->get<std::string_view>();
          }
          auto ownerKoNameIt = accountOwnerPartIt->find("koName");
          if (ownerKoNameIt != accountOwnerPartIt->end()) {
            ownerKoName = ownerKoNameIt->get<std::string_view>();
          }
        }

        map[publicExchangeName].emplace_back(publicExchangeName, name, std::move(keyIt->get_ref<string&>()),
                                             std::move(privateIt->get_ref<string&>()), std::move(passphrase),
                                             AccountOwner(ownerEnName, ownerKoName));
      }
    }
    if (map.empty()) {
      log::warn("No private api keys file '{}' found. Only public exchange queries will be supported", secretFileName);
    }
  }

  return map;
}

string APIKeysProvider::str() const {
  string foundKeysStr;
  for (const auto& [platform, keys] : _apiKeysMap) {
    if (!foundKeysStr.empty()) {
      foundKeysStr.append(" | ");
    }
    if (keys.size() > 1U) {
      foundKeysStr.push_back('{');
    }
    bool firstKey = true;
    for (const APIKey& key : keys) {
      if (!firstKey) {
        foundKeysStr.push_back(',');
      }
      foundKeysStr.append(key.name());
      firstKey = false;
    }
    if (keys.size() > 1U) {
      foundKeysStr.push_back('}');
    }
    foundKeysStr.push_back('@');
    foundKeysStr.append(platform);
  }
  return foundKeysStr;
}

}  // namespace cct::api