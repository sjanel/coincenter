#pragma once

#include <map>
#include <string_view>

#include "apikey.hpp"
#include "cct_const.hpp"
#include "cct_run_modes.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "exchangename.hpp"

namespace cct {
namespace api {
class APIKeysProvider {
 public:
  using KeyNames = SmallVector<string, kTypicalNbPrivateAccounts>;

  explicit APIKeysProvider(std::string_view dataDir, settings::RunMode runMode = settings::RunMode::kProd)
      : APIKeysProvider(dataDir, PublicExchangeNames(), false, runMode) {}

  APIKeysProvider(std::string_view dataDir, const PublicExchangeNames &exchangesWithoutSecrets,
                  bool allExchangesWithoutSecrets, settings::RunMode runMode = settings::RunMode::kProd)
      : _apiKeysMap(ParseAPIKeys(dataDir, exchangesWithoutSecrets, allExchangesWithoutSecrets, runMode)) {}

  APIKeysProvider(const APIKeysProvider &) = delete;
  APIKeysProvider &operator=(const APIKeysProvider &) = delete;

  APIKeysProvider(APIKeysProvider &&) = default;
  APIKeysProvider &operator=(APIKeysProvider &&) = default;

  KeyNames getKeyNames(std::string_view platform) const;

  bool contains(std::string_view platform) const { return _apiKeysMap.find(platform) != _apiKeysMap.end(); }

  const APIKey &get(const PrivateExchangeName &privateExchangeName) const;

 private:
  using APIKeys = vector<APIKey>;
  using APIKeysMap = std::map<string, APIKeys, std::less<>>;

  static APIKeysMap ParseAPIKeys(std::string_view dataDir, const PublicExchangeNames &exchangesWithoutSecrets,
                                 bool allExchangesWithoutSecrets, settings::RunMode runMode);

  APIKeysMap _apiKeysMap;
};
}  // namespace api
}  // namespace cct