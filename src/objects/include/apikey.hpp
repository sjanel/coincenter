#pragma once

#include <string_view>

#include "cct_string.hpp"

namespace cct {
namespace api {
class APIKey {
 public:
  APIKey(std::string_view platform, std::string_view name, string &&key, string &&privateKey, string &&passphrase);

  APIKey(const APIKey &) = delete;
  APIKey operator=(const APIKey &) = delete;
  APIKey(APIKey &&) = default;
  APIKey &operator=(APIKey &&) = default;

  std::string_view platform() const { return _platform; }
  std::string_view name() const { return _name; }
  const string &key() const { return _key; }
  const char *privateKey() const { return _privateKey.c_str(); }
  std::string_view passphrase() const { return _passphrase; }

  ~APIKey();

 private:
  string _platform;
  string _name;
  string _key;
  string _privateKey;
  string _passphrase;
};
}  // namespace api
}  // namespace cct