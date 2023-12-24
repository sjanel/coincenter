#include "proto-market-data-deserializer.hpp"

#include <string_view>

#include "market-order-book-vector.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "proto-constants.hpp"
#include "public-trade-vector.hpp"
#include "serialization-tools.hpp"
#include "time-window.hpp"

namespace cct {
ProtoMarketDataDeserializer::ProtoMarketDataDeserializer(std::string_view dataDir, std::string_view exchangeName)
    : _marketOrderBookDeserializer(ComputeProtoSubPath(dataDir, exchangeName, kSubPathMarketOrderBooks)),
      _publicTradeDeserializer(ComputeProtoSubPath(dataDir, exchangeName, kSubPathTrades)) {}

MarketTimestampSet ProtoMarketDataDeserializer::pullMarketOrderBooksMarkets(TimeWindow timeWindow) {
  return _publicTradeDeserializer.listMarkets(timeWindow);
}

MarketTimestampSet ProtoMarketDataDeserializer::pullTradeMarkets(TimeWindow timeWindow) {
  return _marketOrderBookDeserializer.listMarkets(timeWindow);
}

MarketOrderBookVector ProtoMarketDataDeserializer::pullMarketOrderBooks(Market market, TimeWindow timeWindow) {
  return _marketOrderBookDeserializer.loadMarket(market, timeWindow);
}

PublicTradeVector ProtoMarketDataDeserializer::pullTrades(Market market, TimeWindow timeWindow) {
  return _publicTradeDeserializer.loadMarket(market, timeWindow);
}
}  // namespace cct