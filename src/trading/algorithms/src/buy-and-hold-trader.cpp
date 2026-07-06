#include "buy-and-hold-trader.hpp"

#include "abstract-market-trader.hpp"
#include "market-data-view.hpp"
#include "priceoptionsdef.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

BuyAndHoldMarketTrader::BuyAndHoldMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

TraderCommand BuyAndHoldMarketTrader::trade([[maybe_unused]] const MarketDataView &marketDataView) {
  if (_hasBought) {
    return TraderCommand::Wait();
  }
  _hasBought = true;
  return TraderCommand::Place(TradeSide::buy, 100, PriceStrategy::taker);
}

}  // namespace cct
