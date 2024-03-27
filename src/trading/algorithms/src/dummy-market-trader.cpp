#include "dummy-market-trader.hpp"

#include <span>

#include "abstract-market-trader.hpp"
#include "market-trader-engine-state.hpp"
#include "marketorderbook.hpp"
#include "publictrade.hpp"
#include "trader-command.hpp"

namespace cct {

DummyMarketTrader::DummyMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

TraderCommand DummyMarketTrader::trade([[maybe_unused]] const MarketOrderBook &marketOrderBook,
                                       [[maybe_unused]] std::span<const PublicTrade> trades) {
  return TraderCommand::Wait();
}

}  // namespace cct