#include "proto-market-data-deserializer.hpp"

#include <string_view>

#include "market-order-book-vector.hpp"
#include "market-vector.hpp"
#include "proto-constants.hpp"
#include "public-trade-vector.hpp"
#include "serialization-tools.hpp"
#include "time-window.hpp"

namespace cct {
ProtoMarketDataDeserializer::ProtoMarketDataDeserializer(std::string_view dataDir, std::string_view exchangeName)
    : _marketOrderBookDataGateway(ComputeProtoSubPath(dataDir, exchangeName, kSubPathMarketOrderBook)),
      _tradeDataGateway(ComputeProtoSubPath(dataDir, exchangeName, kSubPathTrades)) {}

MarketVector ProtoMarketDataDeserializer::pullMarketOrderBooksMarkets(TimeWindow timeWindow) {
  return _tradeDataGateway.listMarkets(timeWindow);
}

MarketVector ProtoMarketDataDeserializer::pullTradeMarkets(TimeWindow timeWindow) {
  return _marketOrderBookDataGateway.listMarkets(timeWindow);
}

MarketOrderBookVector ProtoMarketDataDeserializer::pullMarketOrderBooks(Market market, TimeWindow timeWindow) {
  return _marketOrderBookDataGateway.loadMarket(market, timeWindow);
}

PublicTradeVector ProtoMarketDataDeserializer::pullTrades(Market market, TimeWindow timeWindow) {
  return _tradeDataGateway.loadMarket(market, timeWindow);
}
}  // namespace cct