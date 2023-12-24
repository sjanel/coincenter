#pragma once

#include <compare>

#include "market.hpp"
#include "timedef.hpp"

namespace cct {

struct MarketTimestamp {
  Market market;
  TimePoint timePoint;

  std::strong_ordering operator<=>(const MarketTimestamp &) const noexcept = default;
};

}  // namespace cct