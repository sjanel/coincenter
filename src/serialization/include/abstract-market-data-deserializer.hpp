#pragma once

#include "market-order-book-vector.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"

namespace cct {

class AbstractMarketDataDeserializer {
 public:
  virtual ~AbstractMarketDataDeserializer() = default;

  virtual MarketTimestampSet pullMarketOrderBooksMarkets(TimeWindow timeWindow) = 0;

  virtual MarketTimestampSet pullTradeMarkets(TimeWindow timeWindow) = 0;

  virtual MarketOrderBookVector pullMarketOrderBooks(Market market, TimeWindow timeWindow) = 0;

  virtual PublicTradeVector pullTrades(Market market, TimeWindow timeWindow) = 0;
};

}  // namespace cct