#pragma once

#include <utility>

#include "cct_fixedcapacityvector.hpp"
#include "cct_json.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "exchange-name-enum.hpp"

namespace cct::schema {
struct AccountOwner {
  string enName;
  string koName;
};

struct APIKey {
  string key;
  string priv;  // private is a reserved keyword - we override the json field name below
  string passphrase;
  AccountOwner accountOwner;
  bool enabled = true;
};

using APIKeys = SmallVector<std::pair<string, APIKey>, 1>;

using APIKeysPerExchange = FixedCapacityVector<std::pair<ExchangeNameEnum, APIKeys>, kNbSupportedExchanges>;

}  // namespace cct::schema

template <>
struct glz::meta<::cct::schema::APIKey> {
  using V = ::cct::schema::APIKey;
  static constexpr auto value = object("key", &V::key, "private", &V::priv, "passphrase", &V::passphrase,
                                       "accountOwner", &V::accountOwner, "enabled", &V::enabled);
};