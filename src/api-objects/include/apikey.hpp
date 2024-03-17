#pragma once

#include <string_view>
#include <type_traits>

#include "accountowner.hpp"
#include "cct_string.hpp"

namespace cct::api {

class APIKey {
 public:
  /// @brief Creates an API key without an associated name.
  /// @param platform name of the platform exchange in lower case
  /// @param name name of the key as defined in the secret json file
  /// @param key the public api key
  /// @param privateKey the private api key
  /// @param passphrase passphrase used
  APIKey(std::string_view platform, std::string_view name, string &&key, string &&privateKey, string &&passphrase)
      : _platform(platform),
        _name(name),
        _key(std::move(key)),
        _privateKey(std::move(privateKey)),
        _passphrase(std::move(passphrase)) {}

  /// @brief Creates an API key with an associated AccountOwner, needed for Bithumb withdrawals for instance.
  /// @param platform name of the platform exchange in lower case
  /// @param name name of the key as defined in the secret json file
  /// @param key the public api key
  /// @param privateKey the private api key
  /// @param passphrase passphrase used
  /// @param accountOwner the person's name spelled in English that owns the account associated to the key
  APIKey(std::string_view platform, std::string_view name, string &&key, string &&privateKey, string &&passphrase,
         const AccountOwner &accountOwner)
      : _platform(platform),
        _name(name),
        _key(std::move(key)),
        _privateKey(std::move(privateKey)),
        _passphrase(std::move(passphrase)),
        _accountOwner(accountOwner) {}

  APIKey(const APIKey &) = delete;
  APIKey operator=(const APIKey &) = delete;

  APIKey(APIKey &&) noexcept = default;
  APIKey &operator=(APIKey &&) noexcept = default;

  ~APIKey() {
    // force memory clean-up of sensitive information
    _privateKey.assign(_privateKey.size(), '\0');
    _passphrase.assign(_passphrase.size(), '\0');
  }

  std::string_view platform() const { return _platform; }
  std::string_view name() const { return _name; }
  std::string_view key() const { return _key; }
  std::string_view privateKey() const { return _privateKey; }
  std::string_view passphrase() const { return _passphrase; }
  const AccountOwner &accountOwner() const { return _accountOwner; }

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<string> && is_trivially_relocatable_v<AccountOwner>>::type;

 private:
  string _platform;
  string _name;
  string _key;
  string _privateKey;
  string _passphrase;
  AccountOwner _accountOwner;
};
}  // namespace cct::api