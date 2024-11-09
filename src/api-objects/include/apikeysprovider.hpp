#pragma once

#include <array>
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

  KeyNames getKeyNames(ExchangeNameEnum exchangeNameEnum) const;

  bool hasAtLeastOneKey(ExchangeNameEnum exchangeNameEnum) const {
    return !_apiKeysPerExchange[static_cast<int>(exchangeNameEnum)].empty();
  }

  const APIKey &get(const ExchangeName &exchangeName) const;

  string str() const;

 private:
  using APIKeys = vector<APIKey>;
  using APIKeysPerExchange = std::array<APIKeys, kNbSupportedExchanges>;

  static APIKeysPerExchange ParseAPIKeys(std::string_view dataDir, const ExchangeSecretsInfo &exchangeSecretsInfo,
                                         settings::RunMode runMode);

  APIKeysPerExchange _apiKeysPerExchange;
};
}  // namespace cct::api
