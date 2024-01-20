#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

namespace cct {
enum class PriceStrategy : int8_t {
  kMaker,   // Place order at limit price.
  kNibble,  // Buy at 'limit + 1' price, sell at 'limit - 1' price (+-1 referring to previous or next price of the
            // orderbook). Benefits: you control the price, while at the same time speeding up the order execution
            // (compared to kMaker)
  kTaker    // Place order at market price for an expecting direct match
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