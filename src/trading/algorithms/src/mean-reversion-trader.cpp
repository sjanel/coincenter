#include "mean-reversion-trader.hpp"

#include <optional>
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

MeanReversionMarketTrader::MeanReversionMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept
    : AbstractMarketTrader(kName, marketTraderEngineState) {}

MeanReversionMarketTrader::MeanReversionMarketTrader(std::string_view name,
                                                     const MarketTraderEngineState &marketTraderEngineState,
                                                     const Parameters &parameters) noexcept
    : AbstractMarketTrader(name, marketTraderEngineState), _parameters(parameters) {}

TraderCommand MeanReversionMarketTrader::trade(const MarketDataView &marketDataView) {
  const MarketOrderBook &orderBook = marketDataView.currentMarketOrderBook();
  const TimePoint now = orderBook.time();

  // Throttle the (expensive) z-score evaluation to the decision interval.
  if (now - _lastDecisionTime < _parameters.decisionInterval) {
    return TraderCommand::Wait();
  }
  _lastDecisionTime = now;

  const std::optional<MonetaryAmount> optMid = orderBook.averagePrice();
  if (!optMid) {
    return TraderCommand::Wait();
  }

  const TimePoint oldestTime = now - _parameters.lookback;

  // Require the full lookback window to be covered before trading (warm-up at each replay chunk start).
  if (marketDataView.pastMarketOrderBooks().front().time() > oldestTime) {
    return TraderCommand::Wait();
  }

  const BasicStats basicStats(marketDataView);
  const MonetaryAmount movingAverage =
      basicStats.movingAverageFromMarketOrderBooks(oldestTime, _parameters.minSamplingPeriod);
  const MonetaryAmount standardDeviation =
      basicStats.standardDeviationFromMarketOrderBooks(oldestTime, _parameters.minSamplingPeriod);

  const double sigma = standardDeviation.toDouble();
  if (sigma <= 0.0) {
    return TraderCommand::Wait();
  }

  const double zScore = (optMid->toDouble() - movingAverage.toDouble()) / sigma;

  if (zScore <= -_parameters.entryZ && _side != 1) {
    // Price is cheap relative to its mean and we are not already heavy base: buy base with available quote.
    _side = 1;
    return TraderCommand::Place(TradeSide::buy, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  if (zScore >= _parameters.entryZ && _side != -1) {
    // Price is expensive relative to its mean and we are not already heavy quote: sell base for quote.
    _side = -1;
    return TraderCommand::Place(TradeSide::sell, _parameters.intensityPercent, _parameters.priceStrategy);
  }
  return TraderCommand::Wait();
}

}  // namespace cct
