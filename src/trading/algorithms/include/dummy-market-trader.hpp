#pragma once

#include "abstract-market-trader.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

class DummyMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "dummy-trader";

  DummyMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  TraderCommand trade([[maybe_unused]] const MarketDataView &marketDataView) override;
};

}  // namespace cct