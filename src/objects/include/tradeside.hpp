#pragma once

#include <cstdint>
#include <string_view>

#include "unreachable.hpp"

namespace cct {
enum class TradeSide : int8_t { kBuy, kSell };

inline std::string_view SideStr(TradeSide side) {
  switch (side) {
    case TradeSide::kBuy:
      return "Buy";
    case TradeSide::kSell:
      return "Sell";
    default:
      unreachable();
  }
}
}  // namespace cct