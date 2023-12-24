#pragma once

#include "cct_flatset.hpp"
#include "market-timestamp.hpp"

namespace cct {

using MarketTimestampSet = FlatSet<MarketTimestamp>;

struct MarketTimestampSets {
  MarketTimestampSet orderBooksMarkets;
  MarketTimestampSet tradesMarkets;
};

}  // namespace cct