#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "priceoptionsdef.hpp"
#include "timedef.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

/// Trend-following moving-average crossover.
/// Compares a short and a long moving average of the mid price. Buys base when the short average rises
/// sufficiently above the long one (uptrend), sells when it falls sufficiently below (downtrend).
/// A minimum separation ratio creates a neutral band to reduce whipsaws in flat markets.
class MovingAverageCrossoverMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "ma-crossover";

  struct Parameters {
    Duration shortWindow = std::chrono::minutes(15);
    Duration longWindow = std::chrono::hours(2);
    Duration minSamplingPeriod = std::chrono::seconds(0);
    // Only (re)evaluate the (expensive) moving averages this often; bounds CPU cost on dense data.
    Duration decisionInterval = std::chrono::seconds(60);
    double minSeparationRatio = 0.001;  // 0.1% band around the long average to avoid whipsaws
    int8_t intensityPercent = 100;
    PriceStrategy priceStrategy = PriceStrategy::nibble;
  };

  explicit MovingAverageCrossoverMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  MovingAverageCrossoverMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                                     const Parameters &parameters) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  Parameters _parameters;
  TimePoint _lastDecisionTime;
};

}  // namespace cct
