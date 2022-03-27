#pragma once

#include <map>
#include <string_view>

#include "apikey.hpp"
#include "cct_const.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "exchangename.hpp"
#include "exchangesecretsinfo.hpp"
#include "runmodes.hpp"

namespace cct::api {

class APIKeysProvider {
 public:
  using KeyNames = SmallVector<string, kTypicalNbPrivateAccounts>;

  explicit APIKeysProvider(std::string_view dataDir, settings::RunMode runMode = settings::RunMode::kProd)
      : APIKeysProvider(dataDir, ExchangeSecretsInfo(), runMode) {}

  APIKeysProvider(std::string_view dataDir, const ExchangeSecretsInfo &exchangeSecretsInfo,
                  settings::RunMode runMode = settings::RunMode::kProd)
      : _apiKeysMap(ParseAPIKeys(dataDir, exchangeSecretsInfo, runMode)) {}

  APIKeysProvider(const APIKeysProvider &) = delete;
  APIKeysProvider &operator=(const APIKeysProvider &) = delete;

  APIKeysProvider(APIKeysProvider &&) noexcept = default;
  APIKeysProvider &operator=(APIKeysProvider &&) noexcept = default;

  KeyNames getKeyNames(std::string_view platform) const;

  bool contains(std::string_view platform) const { return _apiKeysMap.find(platform) != _apiKeysMap.end(); }

  const APIKey &get(const ExchangeName &exchangeName) const;

 private:
  using APIKeys = vector<APIKey>;
  using APIKeysMap = std::map<string, APIKeys, std::less<>>;

  static APIKeysMap ParseAPIKeys(std::string_view dataDir, const ExchangeSecretsInfo &exchangeSecretsInfo,
                                 settings::RunMode runMode);

  APIKeysMap _apiKeysMap;
};
}  // namespace cct::api