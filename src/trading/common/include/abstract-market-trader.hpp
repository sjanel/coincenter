#pragma once

#include <string_view>

#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

/// Base class for a trading algorithm.
/// It can be derived and only need to implement the trade method to be used in the MarketTraderEngine.
/// the Market Trader Engine state is also provided as a const reference to have the data of the context (orders,
/// available amounts).
class AbstractMarketTrader {
 public:
  virtual ~AbstractMarketTrader() = default;

  virtual TraderCommand trade(const MarketDataView &marketDataView) = 0;

  std::string_view name() const { return _name; }

  const MarketTraderEngineState &marketTraderEngineState() const { return _marketTraderEngineState; }

 protected:
  /// Constructs a new AbstractMarketTrader.
  /// @param name should be a view to a constant string as only a std::string_view will be stored in this object.
  AbstractMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState) noexcept;

 private:
  std::string_view _name;
  const MarketTraderEngineState &_marketTraderEngineState;
};

}  // namespace cct