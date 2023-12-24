#include "query-result-type-helpers.hpp"

#include <algorithm>
#include <iterator>

#include "exchangepublicapitypes.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {

bool ContainsMarket(Market market, const MarketTimestampSet &marketTimestampSet) {
  auto it = std::ranges::partition_point(
      marketTimestampSet, [market](const auto &marketTimestamp) { return marketTimestamp.market < market; });
  return it != marketTimestampSet.end() && it->market == market;
}

bool ContainsMarket(Market market, const MarketTimestampSets &marketTimestampSets) {
  return ContainsMarket(market, marketTimestampSets.orderBooksMarkets) ||
         ContainsMarket(market, marketTimestampSets.tradesMarkets);
}

MarketSet ComputeAllMarkets(const MarketTimestampSetsPerExchange &marketTimestampSetsPerExchange) {
  MarketSet allMarkets;
  for (const auto &[_, marketTimestamps] : marketTimestampSetsPerExchange) {
    std::ranges::transform(marketTimestamps.orderBooksMarkets, std::inserter(allMarkets, allMarkets.end()),
                           [](const auto &marketTimestamp) { return marketTimestamp.market; });
    std::ranges::transform(marketTimestamps.tradesMarkets, std::inserter(allMarkets, allMarkets.end()),
                           [](const auto &marketTimestamp) { return marketTimestamp.market; });
  }
  return allMarkets;
}
}  // namespace cct