#include "apikeysprovider.hpp"

#include <iterator>

#include "cct_exception.hpp"
#include "cct_file.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"

namespace cct::api {
namespace {

std::string_view GetSecretFileName(settings::RunMode runMode) {
  switch (runMode) {
    case settings::RunMode::kTest:
      log::info("Test mode activated, shifting to secret_test.json file.");
      return "secret_test.json";
    default:
      return "secret.json";
  }
}

string FoundKeysStr(const auto& map) {
  string foundKeysStr;
  for (const auto& [platform, keys] : map) {
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
}  // namespace

APIKeysProvider::KeyNames APIKeysProvider::getKeyNames(std::string_view platform) const {
  KeyNames keyNames;
  auto foundIt = _apiKeysMap.find(platform);
  if (foundIt != _apiKeysMap.end()) {
    const APIKeys& apiKeys = foundIt->second;
    std::ranges::transform(apiKeys, std::back_inserter(keyNames),
                           [](const APIKey& apiKey) { return string(apiKey.name()); });
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
                     runMode == settings::RunMode::kProd ? File::IfError::kNoThrow : File::IfError::kThrow);
    json jsonData = secretsFile.readJson();
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
        if (keyIt != keySecretObj.end() && privateIt != keySecretObj.end()) {
          string passphrase;
          auto passphraseIt = keySecretObj.find("passphrase");
          if (passphraseIt != keySecretObj.end()) {
            passphrase = std::move(passphraseIt->get_ref<string&>());
          }
          map[publicExchangeName].emplace_back(publicExchangeName, name, std::move(keyIt->get_ref<string&>()),
                                               std::move(privateIt->get_ref<string&>()), std::move(passphrase));
        } else {
          log::error("Wrong format for secret.json file. It should contain at least fields 'key' and 'private'");
        }
      }
    }
    if (map.empty()) {
      log::warn("No private api keys file '{}' found. Only public exchange queries will be supported", secretFileName);
    }
  }

  if (log::get_level() <= log::level::info) {
    string foundKeysStr = FoundKeysStr(map);
    if (!foundKeysStr.empty()) {
      log::info("Loaded keys {}", foundKeysStr);
    }
  }

  return map;
}
}  // namespace cct::api