#pragma once

#include "exchangepublicapitypes.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {

bool ContainsMarket(Market market, const MarketTimestampSet &marketTimestampSet);

bool ContainsMarket(Market market, const MarketTimestampSets &marketTimestampSets);

MarketSet ComputeAllMarkets(const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange);

}  // namespace cct