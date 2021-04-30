#pragma once

#include <string>
#include <string_view>
#include <unordered_map>

#include "apikey.hpp"
#include "cct_run_modes.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"

namespace cct {

class PrivateExchangeName;

namespace api {
class APIKeysProvider {
 public:
  using KeyNames = cct::SmallVector<std::string, 3>;

  APIKeysProvider(settings::RunMode runMode = settings::kProd) : _apiKeysMap(ParseAPIKeys(runMode)) {}

  APIKeysProvider(const APIKeysProvider &) = delete;
  APIKeysProvider &operator=(const APIKeysProvider &) = delete;

  APIKeysProvider(APIKeysProvider &&) = default;
  APIKeysProvider &operator=(APIKeysProvider &&) = default;

  KeyNames getKeyNames(std::string_view platform) const;

  bool contains(std::string_view platform) const {
    return _apiKeysMap.find(std::string(platform)) != _apiKeysMap.end();
  }

  const APIKey &get(const PrivateExchangeName &privateExchangeName) const;

 private:
  using APIKeys = cct::vector<APIKey>;
  using APIKeysMap = std::unordered_map<std::string, APIKeys>;

  static APIKeysMap ParseAPIKeys(settings::RunMode runMode);

  /// const to ensure permanent validity on references to APIKeys.
  /// This is to avoid copying some sensitive information elsewhere in the program.
  const APIKeysMap _apiKeysMap;
};
}  // namespace api
}  // namespace cct