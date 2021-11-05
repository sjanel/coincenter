#pragma once

#include <string_view>

#include "cct_string.hpp"

namespace cct {
namespace api {
class APIKey {
 public:
  APIKey(std::string_view platform, std::string_view name, string &&key, string &&privateKey, string &&passphrase)
      : _platform(platform),
        _name(name),
        _key(std::move(key)),
        _privateKey(std::move(privateKey)),
        _passphrase(std::move(passphrase)) {}

  APIKey(const APIKey &) = delete;
  APIKey operator=(const APIKey &) = delete;
  APIKey(APIKey &&) = default;
  APIKey &operator=(APIKey &&) = default;

  std::string_view platform() const { return _platform; }
  std::string_view name() const { return _name; }
  std::string_view key() const { return _key; }
  std::string_view privateKey() const { return _privateKey; }
  std::string_view passphrase() const { return _passphrase; }

  ~APIKey() { _privateKey.assign(_privateKey.size(), '\0'); }

 private:
  string _platform;
  string _name;
  string _key;
  string _privateKey;
  string _passphrase;
};
}  // namespace api
}  // namespace cct