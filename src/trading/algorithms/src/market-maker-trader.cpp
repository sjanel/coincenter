#include "market-maker-trader.hpp"

#include <optional>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "market-data-view.hpp"
#include "market-trader-engine-state.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "priceoptionsdef.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

MarketMakerMarketTrader::MarketMakerMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

MarketMakerMarketTrader::MarketMakerMarketTrader(std::string_view name,
                                                 const MarketTraderEngineState &marketTraderEngineState,
                                                 const Parameters &parameters) noexcept
    : AbstractMarketTrader(name, marketTraderEngineState), _parameters(parameters) {}

TraderCommand MarketMakerMarketTrader::trade(const MarketDataView &marketDataView) {
  const MarketTraderEngineState &state = marketTraderEngineState();

  // Let a resting order rest until it gets matched by the engine.
  if (!state.openedOrders().empty()) {
    return TraderCommand::Wait();
  }

  const MarketOrderBook &orderBook = marketDataView.currentMarketOrderBook();
  const std::optional<MonetaryAmount> optMid = orderBook.averagePrice();
  if (!optMid) {
    return TraderCommand::Wait();
  }

  const double mid = optMid->toDouble();
  const double baseValue = state.availableBaseAmount().toDouble() * mid;
  const double quoteValue = state.availableQuoteAmount().toDouble();

  // Hold mostly quote -> post a maker buy at the bid; hold mostly base -> post a maker sell at the ask.
  if (quoteValue > baseValue) {
    return TraderCommand::Place(TradeSide::buy, _parameters.intensityPercent, PriceStrategy::maker);
  }
  return TraderCommand::Place(TradeSide::sell, _parameters.intensityPercent, PriceStrategy::maker);
}

}  // namespace cct
