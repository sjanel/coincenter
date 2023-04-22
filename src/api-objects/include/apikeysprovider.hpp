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
  using KeyNames = SmallVector<std::string_view, kTypicalNbPrivateAccounts>;

  APIKeysProvider(std::string_view dataDir, settings::RunMode runMode)
      : APIKeysProvider(dataDir, ExchangeSecretsInfo(), runMode) {}

  APIKeysProvider(std::string_view dataDir, const ExchangeSecretsInfo &exchangeSecretsInfo, settings::RunMode runMode);

  KeyNames getKeyNames(std::string_view platform) const;

  bool contains(std::string_view platform) const { return _apiKeysMap.find(platform) != _apiKeysMap.end(); }

  const APIKey &get(const ExchangeName &exchangeName) const;

  string str() const;

 private:
  using APIKeys = vector<APIKey>;
  using APIKeysMap = std::map<string, APIKeys, std::less<>>;

  static APIKeysMap ParseAPIKeys(std::string_view dataDir, const ExchangeSecretsInfo &exchangeSecretsInfo,
                                 settings::RunMode runMode);

  APIKeysMap _apiKeysMap;
};
}  // namespace cct::api