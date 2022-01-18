#include "apikeysprovider.hpp"

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
      break;
  };
  return "secret.json";
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

const APIKey& APIKeysProvider::get(const PrivateExchangeName& privateExchangeName) const {
  std::string_view platformStr = privateExchangeName.name();
  auto foundIt = _apiKeysMap.find(platformStr);
  if (foundIt == _apiKeysMap.end()) {
    string ex("Unable to retrieve private key for ");
    ex.append(platformStr);
    throw exception(std::move(ex));
  }
  const APIKeys& apiKeys = foundIt->second;
  if (!privateExchangeName.isKeyNameDefined()) {
    if (apiKeys.size() > 1) {
      string ex("Specify name for ");
      ex.append(platformStr).append(" keys as you have several");
      throw exception(std::move(ex));
    }
    return apiKeys.front();
  }
  auto keyNameIt = std::ranges::find_if(
      apiKeys, [privateExchangeName](const APIKey& apiKey) { return apiKey.name() == privateExchangeName.keyName(); });
  if (keyNameIt == apiKeys.end()) {
    string ex("Unable to retrieve private key for ");
    ex.append(platformStr).append(" named ").append(privateExchangeName.keyName());
    throw exception(std::move(ex));
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
                     runMode == settings::RunMode::kProd ? File::IfNotFound::kNoThrow : File::IfNotFound::kThrow);
    json jsonData = secretsFile.readJson();
    for (const auto& [platform, keyObj] : jsonData.items()) {
      const auto& exchangesWithoutSecrets = exchangeSecretsInfo.exchangesWithoutSecrets();
      if (std::ranges::find(exchangesWithoutSecrets, platform) != exchangesWithoutSecrets.end()) {
        log::info("Not loading {} private keys as requested", platform);
        continue;
      }
      for (const auto& [name, keySecretObj] : keyObj.items()) {
        if (keySecretObj.contains("key") && keySecretObj.contains("private")) {
          string passphrase;
          if (keySecretObj.contains("passphrase")) {
            passphrase = keySecretObj["passphrase"];
          }
          map[platform].emplace_back(platform, name, keySecretObj["key"], keySecretObj["private"],
                                     std::move(passphrase));
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
    if (!foundKeysStr.empty()) {
      log::info("Loaded keys {}", foundKeysStr);
    }
  }

  return map;
}
}  // namespace cct::api