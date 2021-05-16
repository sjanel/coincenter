#include "apikeysprovider.hpp"

#include <fstream>
#include <streambuf>
#include <string>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_json.hpp"
#include "cct_log.hpp"
#include "exchangename.hpp"

namespace cct {
namespace api {
namespace {

std::string_view GetSecretFileName(settings::RunMode runMode) {
  switch (runMode) {
    case settings::RunMode::kTest:
      log::info("Test mode activated, shifting to secret_test.json file.");
      return "/secret_test.json";
    default:
      break;
  };
  return "/secret.json";
}
}  // namespace

APIKeysProvider::KeyNames APIKeysProvider::getKeyNames(std::string_view platform) const {
  KeyNames keyNames;
  std::string platformStr(platform);
  auto foundIt = _apiKeysMap.find(platformStr);
  if (foundIt != _apiKeysMap.end()) {
    const APIKeys& apiKeys = foundIt->second;
    std::transform(apiKeys.begin(), apiKeys.end(), std::back_inserter(keyNames),
                   [](const APIKey& apiKey) { return std::string(apiKey.name()); });
  }
  return keyNames;
}

const APIKey& APIKeysProvider::get(const PrivateExchangeName& privateExchangeName) const {
  std::string platformStr(privateExchangeName.name());
  auto foundIt = _apiKeysMap.find(platformStr);
  if (foundIt == _apiKeysMap.end()) {
    throw exception("Unable to retrieve private key for " + platformStr);
  }
  const APIKeys& apiKeys = foundIt->second;
  if (!privateExchangeName.isKeyNameDefined()) {
    if (apiKeys.size() > 1) {
      throw exception("Specify name for " + platformStr + " keys as you have several");
    }
    return apiKeys.front();
  }
  auto keyNameIt = std::find_if(apiKeys.begin(), apiKeys.end(), [privateExchangeName](const APIKey& apiKey) {
    return apiKey.name() == privateExchangeName.keyName();
  });
  if (keyNameIt == apiKeys.end()) {
    throw exception("Unable to retrieve private key for " + platformStr + " named " +
                    std::string(privateExchangeName.keyName()));
  }
  return *keyNameIt;
}

APIKeysProvider::APIKeysMap APIKeysProvider::ParseAPIKeys(settings::RunMode runMode) {
  APIKeysProvider::APIKeysMap map;
  std::string secretKeyFile(kDataPath);
  secretKeyFile.append(GetSecretFileName(runMode));
  std::ifstream file(secretKeyFile);
  if (!file) {
    log::warn("No private api keys file at path '{}'. Only public exchange queries will be supported.", secretKeyFile);
  } else {
    std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    json jsonData = json::parse(data);
    for (const auto& [platform, keyObj] : jsonData.items()) {
      for (const auto& [name, keySecretObj] : keyObj.items()) {
        log::warn("Found key '{}' for platform {}", name, platform);
        map[platform].emplace_back(platform, name, keySecretObj["key"], keySecretObj["private"]);
      }
    }
  }
  if (map.empty()) {
    log::warn("No private api keys found, unable to use any private API");
  }
  return map;
}
}  // namespace api
}  // namespace cct