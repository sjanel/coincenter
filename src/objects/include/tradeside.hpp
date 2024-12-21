#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json.hpp"
#include "unreachable.hpp"

namespace cct {
enum class TradeSide : int8_t { buy, sell };
}  // namespace cct

template <>
struct glz::meta<cct::TradeSide> {
  using enum cct::TradeSide;

  static constexpr auto value = enumerate(buy, sell);
};