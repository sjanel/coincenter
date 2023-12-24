#include "dummy-market-data-deserializer.hpp"

#include <string_view>

#include "market-order-book-vector.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"

namespace cct {

DummyMarketDataDeserializer::DummyMarketDataDeserializer([[maybe_unused]] std::string_view dataDir,
                                                         [[maybe_unused]] std::string_view exchangeName) {}

MarketTimestampSet DummyMarketDataDeserializer::pullMarketOrderBooksMarkets([[maybe_unused]] TimeWindow timeWindow) {
  return {};
}

MarketTimestampSet DummyMarketDataDeserializer::pullTradeMarkets([[maybe_unused]] TimeWindow timeWindow) { return {}; }

MarketOrderBookVector DummyMarketDataDeserializer::pullMarketOrderBooks([[maybe_unused]] Market market,
                                                                        [[maybe_unused]] TimeWindow timeWindow) {
  return {};
}

PublicTradeVector DummyMarketDataDeserializer::pullTrades([[maybe_unused]] Market market,
                                                          [[maybe_unused]] TimeWindow timeWindow) {
  return {};
}
}  // namespace cct