#include "dummy-market-trader.hpp"

#include "abstract-market-trader.hpp"
#include "market-data-view.hpp"
#include "market-trader-engine-state.hpp"
#include "trader-command.hpp"

namespace cct {

DummyMarketTrader::DummyMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

TraderCommand DummyMarketTrader::trade([[maybe_unused]] const MarketDataView &marketDataView) {
  return TraderCommand::Wait();
}

}  // namespace cct