#pragma once

#include <memory>
#include <span>
#include <string_view>

#include "abstract-market-trader-factory.hpp"
#include "abstract-market-trader.hpp"

namespace cct {

class MarketTraderEngineState;

class MarketTraderFactory : public AbstractMarketTraderFactory {
 public:
  std::span<const std::string_view> allSupportedAlgorithms() const override;

  /// Creates a new MarketTrader from the underlying type of the algorithm name.
  /// For instance, create("dummy-trader") will return a DummyMarketTrader.
  std::unique_ptr<AbstractMarketTrader> construct(
      std::string_view algorithmName, const MarketTraderEngineState& marketTraderEngineState) const override;
};
}  // namespace cct