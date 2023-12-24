#pragma once

#include "market-trading-result.hpp"
#include "trade-range-stats.hpp"

namespace cct {

struct MarketTradingGlobalResult {
  MarketTradingResult result;
  TradeRangeStats stats;
};

}  // namespace cct