#pragma once

#include "abstract-market-trader.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

class ExampleMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "example-trader";

  ExampleMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  TraderCommand trade([[maybe_unused]] const MarketDataView &marketDataView) override;
};

}  // namespace cct