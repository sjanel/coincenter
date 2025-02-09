#pragma once

#include <type_traits>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::binance {

struct NetworkCoinData {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const NetworkCoinData&) const = default;

  struct NetworkListElement {
    auto operator<=>(const NetworkListElement&) const = default;

    bool isDefault;
    bool depositEnable;
    bool withdrawEnable;
    MonetaryAmount withdrawFee;
  };

  string coin;
  bool isLegalMoney;
  SmallVector<NetworkListElement, 4> networkList;
};

using NetworkCoinDataVector = vector<NetworkCoinData>;

struct NetworkCoinAll {
  string code;
  NetworkCoinDataVector data;
};

}  // namespace cct::schema::binance