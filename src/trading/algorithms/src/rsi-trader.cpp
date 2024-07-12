#include "rsi-trader.hpp"

#include <string_view>

#include "abstract-market-trader.hpp"
#include "basic-stats.hpp"
#include "market-data-view.hpp"
#include "marketorderbook.hpp"
#include "timedef.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

RsiMarketTrader::RsiMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

RsiMarketTrader::RsiMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                                 const Parameters &parameters) noexcept
    : AbstractMarketTrader(name, marketTraderEngineState), _parameters(parameters) {}

TraderCommand RsiMarketTrader::trade(const MarketDataView &marketDataView) {
  const MarketOrderBook &orderBook = marketDataView.currentMarketOrderBook();
  const TimePoint now = orderBook.time();

  // Throttle the (expensive) RSI evaluation to the decision interval.
  if (now - _lastDecisionTime < _parameters.decisionInterval) {
    return TraderCommand::Wait();
  }
  _lastDecisionTime = now;

  // Need rsiPeriods deltas, i.e. (rsiPeriods + 1) samples spread over the lookback window.
  const TimePoint oldestTime = now - (_parameters.rsiPeriods + 1) * _parameters.samplingPeriod;
  if (marketDataView.pastMarketOrderBooks().front().time() > oldestTime) {
    return TraderCommand::Wait();
  }

  const BasicStats basicStats(marketDataView);
  const double rsi = basicStats.relativeStrengthIndexFromMarketOrderBooks(oldestTime, _parameters.samplingPeriod);
  if (rsi < 0.0) {
    return TraderCommand::Wait();
  }

  if (rsi <= _parameters.oversold && _side != 1) {
    // Oversold: buy base with available quote.
    _side = 1;
    return TraderCommand::Place(TradeSide::buy, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  if (rsi >= _parameters.overbought && _side != -1) {
    // Overbought: sell base for quote.
    _side = -1;
    return TraderCommand::Place(TradeSide::sell, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  return TraderCommand::Wait();
}

}  // namespace cct
