#include "apikeysprovider.hpp"

#include "cct_allfiles.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "cct_string.hpp"

namespace cct {
namespace api {
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
    std::transform(apiKeys.begin(), apiKeys.end(), std::back_inserter(keyNames),
                   [](const APIKey& apiKey) { return string(apiKey.name()); });
  }
  return keyNames;
}

const APIKey& APIKeysProvider::get(const PrivateExchangeName& privateExchangeName) const {
  std::string_view platformStr = privateExchangeName.name();
  auto foundIt = _apiKeysMap.find(platformStr);
  if (foundIt == _apiKeysMap.end()) {
    throw exception("Unable to retrieve private key for " + string(platformStr));
  }
  const APIKeys& apiKeys = foundIt->second;
  if (!privateExchangeName.isKeyNameDefined()) {
    if (apiKeys.size() > 1) {
      throw exception("Specify name for " + string(platformStr) + " keys as you have several");
    }
    return apiKeys.front();
  }
  auto keyNameIt = std::find_if(apiKeys.begin(), apiKeys.end(), [privateExchangeName](const APIKey& apiKey) {
    return apiKey.name() == privateExchangeName.keyName();
  });
  if (keyNameIt == apiKeys.end()) {
    throw exception("Unable to retrieve private key for " + string(platformStr) + " named " +
                    string(privateExchangeName.keyName()));
  }
  return *keyNameIt;
}

APIKeysProvider::APIKeysMap APIKeysProvider::ParseAPIKeys(const PublicExchangeNames& exchangesWithoutSecrets,
                                                          bool allExchangesWithoutSecrets, settings::RunMode runMode) {
  APIKeysProvider::APIKeysMap map;
  if (allExchangesWithoutSecrets) {
    log::info("Not loading private keys, using only public exchanges");
  } else {
    json jsonData = runMode == settings::RunMode::kProd ? kSecret.readJson() : kSecretTest.readJson();
    for (const auto& [platform, keyObj] : jsonData.items()) {
      if (std::find(exchangesWithoutSecrets.begin(), exchangesWithoutSecrets.end(), platform) !=
          exchangesWithoutSecrets.end()) {
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
          log::info("Found key '{}' for platform {}", name, platform);
        } else {
          log::error("Wrong format for secret.json file. It should contain at least fields 'key' and 'private'");
        }
      }
    }
    if (map.empty()) {
      log::warn("No private api keys file '{}' found. Only public exchange queries will be supported",
                GetSecretFileName(runMode));
    }
  }

  return map;
}
}  // namespace api
}  // namespace cct