#pragma once

#include <span>

#include "market.hpp"
#include "publictrade.hpp"

namespace cct {

class MarketOrderBook;

class AbstractMarketDataSerializer {
 public:
  virtual ~AbstractMarketDataSerializer() = default;

  /// Push market order book in the MarketDataSerializer.
  virtual void push(const MarketOrderBook &marketOrderBook) = 0;

  /// Push public trades in the MarketDataSerializer.
  /// They should come from the same market.
  virtual void push(Market market, std::span<const PublicTrade> publicTrades) = 0;
};

}  // namespace cct