#include "ma-crossover-trader.hpp"

#include <string_view>

#include "abstract-market-trader.hpp"
#include "basic-stats.hpp"
#include "market-data-view.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

MovingAverageCrossoverMarketTrader::MovingAverageCrossoverMarketTrader(
    const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

MovingAverageCrossoverMarketTrader::MovingAverageCrossoverMarketTrader(
    std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
    const Parameters &parameters) noexcept
    : AbstractMarketTrader(name, marketTraderEngineState), _parameters(parameters) {}

TraderCommand MovingAverageCrossoverMarketTrader::trade(const MarketDataView &marketDataView) {
  const MarketOrderBook &orderBook = marketDataView.currentMarketOrderBook();
  const TimePoint now = orderBook.time();

  // Throttle the (expensive) moving-average evaluation to the decision interval.
  if (now - _lastDecisionTime < _parameters.decisionInterval) {
    return TraderCommand::Wait();
  }
  _lastDecisionTime = now;

  const TimePoint oldestLong = now - _parameters.longWindow;

  // Require the full long window to be covered before trading.
  if (marketDataView.pastMarketOrderBooks().front().time() > oldestLong) {
    return TraderCommand::Wait();
  }

  const BasicStats basicStats(marketDataView);
  const double shortMA =
      basicStats.movingAverageFromMarketOrderBooks(now - _parameters.shortWindow, _parameters.minSamplingPeriod)
          .toDouble();
  const double longMA =
      basicStats.movingAverageFromMarketOrderBooks(oldestLong, _parameters.minSamplingPeriod).toDouble();

  if (longMA <= 0.0) {
    return TraderCommand::Wait();
  }

  const double separationRatio = (shortMA - longMA) / longMA;

  if (separationRatio >= _parameters.minSeparationRatio) {
    // Uptrend: buy base.
    return TraderCommand::Place(TradeSide::buy, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  if (separationRatio <= -_parameters.minSeparationRatio) {
    // Downtrend: sell base.
    return TraderCommand::Place(TradeSide::sell, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  return TraderCommand::Wait();
}

}  // namespace cct
