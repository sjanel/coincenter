#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

#include "cct_json.hpp"

namespace cct {
enum class PriceStrategy : int8_t {
  maker,   // Place order at limit price.
  nibble,  // Buy at 'limit + 1' price, sell at 'limit - 1' price (+-1 referring to previous or next price of the
           // orderbook). Benefits: you control the price, while at the same time speeding up the order execution
           // (compared to kMaker)
  taker    // Place order at market price for an expecting direct match
};

PriceStrategy StrategyFromStr(std::string_view priceStrategyStr);

std::string_view PriceStrategyStr(PriceStrategy priceStrategy, bool placeRealOrderInSimulationMode);

/// Extension of above price strategies, to have a more precise control of the pricing behavior.
/// It allows picking a price at a <n> relative step price compared to the ask and bid prices of the orderbook.
/// Negative values corresponds to the 'taker' method, by matching immediately available amounts, positive values is a
/// limit price.
using RelativePrice = int32_t;

static constexpr RelativePrice kNoRelativePrice = std::numeric_limits<RelativePrice>::min();

}  // namespace cct

// To make enum serializable as strings
template <>
struct glz::meta<cct::PriceStrategy> {
  using enum cct::PriceStrategy;

  static constexpr auto value = enumerate(maker, nibble, taker);
};