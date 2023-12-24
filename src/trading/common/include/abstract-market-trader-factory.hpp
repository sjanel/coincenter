#pragma once

#include <memory>
#include <span>
#include <string_view>

namespace cct {

class AbstractMarketTrader;
class MarketTraderEngineState;

/// Interface that you need to derive to provide your own algorithms to coincenter.
class AbstractMarketTraderFactory {
 public:
  /// Returns a span of all supported algorithms of this market trader factory.
  virtual std::span<const std::string_view> allSupportedAlgorithms() const = 0;

  /// Creates a new MarketTrader from the underlying type of the algorithm name.
  /// For instance, create("dummy-trader") will return a DummyMarketTrader.
  virtual std::unique_ptr<AbstractMarketTrader> construct(
      std::string_view algorithmName, const MarketTraderEngineState& marketTraderEngineState) const = 0;
};

}  // namespace cct