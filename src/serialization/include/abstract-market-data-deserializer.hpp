#pragma once

#include "market-order-book-vector.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"

namespace cct {
class AbstractMarketDataDeserializer {
 public:
  virtual ~AbstractMarketDataDeserializer() = default;

  virtual MarketVector pullMarketOrderBooksMarkets(TimeWindow timeWindow) = 0;

  virtual MarketVector pullTradeMarkets(TimeWindow timeWindow) = 0;

  virtual MarketOrderBookVector pullMarketOrderBooks(Market market, TimeWindow timeWindow) = 0;

  virtual PublicTradeVector pullTrades(Market market, TimeWindow timeWindow) = 0;
};
}  // namespace cct