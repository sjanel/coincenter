#pragma once

#include <unordered_map>
#include <utility>

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct::schema {
struct AccountOwner {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  string enName;
  string koName;
};

struct APIKey {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  string key;
  string priv;  // private is a reserved keyword - we override the json field name below
  string passphrase;
  AccountOwner accountOwner;
  bool enabled = true;
};

using APIKeys = vector<std::pair<string, APIKey>>;

using APIKeysPerExchange = std::unordered_map<ExchangeNameEnum, APIKeys>;

}  // namespace cct::schema

template <>
struct glz::meta<::cct::schema::APIKey> {
  using V = ::cct::schema::APIKey;
  static constexpr auto value = object("key", &V::key, "private", &V::priv, "passphrase", &V::passphrase,
                                       "accountOwner", &V::accountOwner, "enabled", &V::enabled);
};