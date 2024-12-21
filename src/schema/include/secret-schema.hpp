#pragma once

#include <unordered_map>

#include "cct_const.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"

namespace cct::schema {

struct AccountOwner {
  string enName;
  string koName;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

struct APIKey {
  string key;
  string priv;  // private is a reserved keyword - we override the json field name below
  string passphrase;
  AccountOwner accountOwner;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using APIKeys = std::unordered_map<string, APIKey>;

using APIKeysPerExchangeMap = std::unordered_map<ExchangeNameEnum, APIKeys>;

}  // namespace cct::schema

template <>
struct glz::meta<::cct::schema::APIKey> {
  using V = ::cct::schema::APIKey;
  static constexpr auto value =
      object("key", &V::key, "private", &V::priv, "passphrase", &V::passphrase, "accountOwner", &V::accountOwner);
};