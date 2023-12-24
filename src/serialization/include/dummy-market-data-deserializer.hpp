#pragma once

#include <string_view>

#include "abstract-market-data-deserializer.hpp"
#include "market-order-book-vector.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"

namespace cct {

class DummyMarketDataDeserializer : public AbstractMarketDataDeserializer {
 public:
  DummyMarketDataDeserializer([[maybe_unused]] std::string_view dataDir,
                              [[maybe_unused]] std::string_view exchangeName);

  MarketTimestampSet pullMarketOrderBooksMarkets([[maybe_unused]] TimeWindow timeWindow) override;

  MarketTimestampSet pullTradeMarkets([[maybe_unused]] TimeWindow timeWindow) override;

  MarketOrderBookVector pullMarketOrderBooks([[maybe_unused]] Market market,
                                             [[maybe_unused]] TimeWindow timeWindow) override;

  PublicTradeVector pullTrades([[maybe_unused]] Market market, [[maybe_unused]] TimeWindow timeWindow) override;
};

}  // namespace cct