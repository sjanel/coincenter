#pragma once

#include <type_traits>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::binance {

struct NetworkListElement {
  bool isDefault{};
  bool depositEnable{};
  bool withdrawEnable{};
  MonetaryAmount withdrawFee;

  auto operator<=>(const NetworkListElement&) const = default;
};

struct NetworkCoinData {
  string coin;
  bool isLegalMoney{};
  SmallVector<NetworkListElement, 4> networkList;

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<string> &&
                         is_trivially_relocatable_v<SmallVector<NetworkListElement, 4>>>::type;

  auto operator<=>(const NetworkCoinData&) const = default;
};

using NetworkCoinDataVector = vector<NetworkCoinData>;

struct NetworkCoinAll {
  string code;
  NetworkCoinDataVector data;
};

}  // namespace cct::schema::binance