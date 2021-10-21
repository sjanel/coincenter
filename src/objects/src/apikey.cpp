#include "apikey.hpp"

#include <algorithm>

namespace cct {
namespace api {
APIKey::APIKey(std::string_view platform, std::string_view name, string &&key, string &&privateKey, string &&passphrase)
    : _platform(platform),
      _name(name),
      _key(std::move(key)),
      _privateKey(std::move(privateKey)),
      _passphrase(std::move(passphrase)) {}

APIKey::~APIKey() { std::fill(_privateKey.begin(), _privateKey.end(), '\0'); }

}  // namespace api
}  // namespace cct