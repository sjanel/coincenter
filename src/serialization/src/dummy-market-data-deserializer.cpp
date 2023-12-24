#include "dummy-market-data-deserializer.hpp"

#include "market-order-book-vector.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"

namespace cct {

DummyMarketDataDeserializer::DummyMarketDataDeserializer([[maybe_unused]] std::string_view dataDir,
                                                         [[maybe_unused]] std::string_view exchangeName) {}

MarketVector DummyMarketDataDeserializer::pullMarketOrderBooksMarkets([[maybe_unused]] TimeWindow timeWindow) {
  return {};
}

MarketVector DummyMarketDataDeserializer::pullTradeMarkets([[maybe_unused]] TimeWindow timeWindow) { return {}; }

MarketOrderBookVector DummyMarketDataDeserializer::pullMarketOrderBooks([[maybe_unused]] Market market,
                                                                        [[maybe_unused]] TimeWindow timeWindow) {
  return {};
}

PublicTradeVector DummyMarketDataDeserializer::pullTrades([[maybe_unused]] Market market,
                                                          [[maybe_unused]] TimeWindow timeWindow) {
  return {};
}
}  // namespace cct