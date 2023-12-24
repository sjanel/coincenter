#include "example-market-trader.hpp"

#include <span>

#include "abstract-market-trader.hpp"
#include "market-trader-engine-state.hpp"
#include "marketorderbook.hpp"
#include "publictrade.hpp"
#include "trader-command.hpp"

namespace cct {

ExampleMarketTrader::ExampleMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

TraderCommand ExampleMarketTrader::trade([[maybe_unused]] const MarketOrderBook &marketOrderBook,
                                         [[maybe_unused]] std::span<const PublicTrade> trades) {
  return TraderCommand::Place(TradeSide::kSell);
}

}  // namespace cct