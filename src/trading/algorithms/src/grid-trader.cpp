#include "grid-trader.hpp"

#include <optional>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "market-data-view.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

GridMarketTrader::GridMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

GridMarketTrader::GridMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                                   const Parameters &parameters) noexcept
    : AbstractMarketTrader(name, marketTraderEngineState), _parameters(parameters) {}

TraderCommand GridMarketTrader::trade(const MarketDataView &marketDataView) {
  const MarketOrderBook &orderBook = marketDataView.currentMarketOrderBook();
  const std::optional<MonetaryAmount> optMid = orderBook.averagePrice();
  if (!optMid) {
    return TraderCommand::Wait();
  }

  const double mid = optMid->toDouble();

  if (_anchorPrice <= 0.0) {
    // First valid observation: set the initial grid anchor and wait.
    _anchorPrice = mid;
    return TraderCommand::Wait();
  }

  if (mid <= _anchorPrice * (1.0 - _parameters.stepRatio)) {
    // Price dropped one grid step: buy a slice and re-anchor.
    _anchorPrice = mid;
    return TraderCommand::Place(TradeSide::buy, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  if (mid >= _anchorPrice * (1.0 + _parameters.stepRatio)) {
    // Price rose one grid step: sell a slice and re-anchor.
    _anchorPrice = mid;
    return TraderCommand::Place(TradeSide::sell, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  return TraderCommand::Wait();
}

}  // namespace cct
