#pragma once

#include <map>
#include <string>
#include <string_view>

#include "apikey.hpp"
#include "cct_const.hpp"
#include "cct_run_modes.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "exchangename.hpp"

namespace cct {
namespace api {
class APIKeysProvider {
 public:
  using KeyNames = SmallVector<std::string, kTypicalNbPrivateAccounts>;

  explicit APIKeysProvider(settings::RunMode runMode = settings::RunMode::kProd)
      : APIKeysProvider(PublicExchangeNames(), false, runMode) {}

  APIKeysProvider(const PublicExchangeNames &exchangesWithoutSecrets, bool allExchangesWithoutSecrets,
                  settings::RunMode runMode = settings::RunMode::kProd)
      : _apiKeysMap(ParseAPIKeys(exchangesWithoutSecrets, allExchangesWithoutSecrets, runMode)) {}

  APIKeysProvider(const APIKeysProvider &) = delete;
  APIKeysProvider &operator=(const APIKeysProvider &) = delete;

  APIKeysProvider(APIKeysProvider &&) = default;
  APIKeysProvider &operator=(APIKeysProvider &&) = default;

  KeyNames getKeyNames(std::string_view platform) const;

  bool contains(std::string_view platform) const { return _apiKeysMap.find(platform) != _apiKeysMap.end(); }

  const APIKey &get(const PrivateExchangeName &privateExchangeName) const;

 private:
  using APIKeys = vector<APIKey>;
  using APIKeysMap = std::map<std::string, APIKeys, std::less<>>;

  static APIKeysMap ParseAPIKeys(const PublicExchangeNames &exchangesWithoutSecrets, bool allExchangesWithoutSecrets,
                                 settings::RunMode runMode);

  APIKeysMap _apiKeysMap;
};
}  // namespace api
}  // namespace cct