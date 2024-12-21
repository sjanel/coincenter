#include "example-market-trader.hpp"

#include "abstract-market-trader.hpp"
#include "market-data-view.hpp"
#include "market-trader-engine-state.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

ExampleMarketTrader::ExampleMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

TraderCommand ExampleMarketTrader::trade([[maybe_unused]] const MarketDataView &marketDataView) {
  return TraderCommand::Place(TradeSide::sell);
}

}  // namespace cct