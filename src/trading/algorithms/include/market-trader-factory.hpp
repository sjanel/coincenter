#pragma once

#include <memory>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "dummy-market-trader.hpp"
#include "example-market-trader.hpp"
#include "static_string_view_helpers.hpp"

namespace cct {

class MarketTraderEngineState;

class MarketTraderFactory {
 public:
  static constexpr std::string_view kAllAlgorithms[] = {DummyMarketTrader::kName, ExampleMarketTrader::kName};

  static constexpr std::string_view kAlgorithmNameSep = ",";

  static constexpr std::string_view kAllAlgorithmsConcatenated =
      make_joined_string_view<kAlgorithmNameSep, kAllAlgorithms>::value;

  explicit MarketTraderFactory(std::string_view algorithmName);

  /// Creates a new MarketTrader from the underlying type of the algorithm name.
  /// For instance, create("dummy-trader") will return a DummyMarketTrader.
  std::unique_ptr<AbstractMarketTrader> construct(const MarketTraderEngineState& marketTraderEngineState) const;

 private:
  std::string_view _algorithmName;
};
}  // namespace cct