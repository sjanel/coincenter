#pragma once

#include <string>
#include <string_view>

namespace cct {
namespace api {
class APIKey {
 public:
  APIKey(std::string_view platform, std::string_view name, std::string &&key, std::string &&privateKey)
      : _platform(platform), _name(name), _key(std::move(key)), _privateKey(std::move(privateKey)) {}

  APIKey(const APIKey &) = delete;
  APIKey operator=(const APIKey &) = delete;
  APIKey(APIKey &&) = default;
  APIKey &operator=(APIKey &&) = default;

  std::string_view platform() const { return _platform; }
  std::string_view name() const { return _name; }
  const std::string &key() const { return _key; }
  const char *privateKey() const { return _privateKey.c_str(); }

  ~APIKey() { std::fill(_privateKey.begin(), _privateKey.end(), '\0'); }

 private:
  std::string _platform;
  std::string _name;
  std::string _key;
  std::string _privateKey;
};
}  // namespace api
}  // namespace cct