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

/// Mean-reversion (z-score) trading algorithm.
/// Computes z = (mid - movingAverage) / standardDeviation over a lookback window.
/// Buys base (spends quote) when the price is significantly below its mean (z <= -entryZ),
/// sells base (gets quote) when significantly above (z >= +entryZ).
/// Long-only spot: it swings between "heavy base" (after an oversold buy) and "heavy quote" (after an
/// overbought sell). It uses hysteresis so it only acts once per threshold crossing and then waits for
/// the opposite extreme - this avoids re-entering on every tick (which would bleed fees).
class MeanReversionMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "mean-reversion";

  struct Parameters {
    // A long lookback + high entry threshold keep the trade frequency (and therefore the fee drag) low,
    // and make the z-score react to meaningful swings rather than tick noise.
    Duration lookback = std::chrono::hours(4);
    Duration minSamplingPeriod = std::chrono::seconds(0);
    // Only (re)evaluate the (relatively expensive) z-score this often; between decision points the trader
    // returns Wait cheaply. This bounds CPU cost on dense data and avoids re-deciding on every tick.
    Duration decisionInterval = std::chrono::seconds(60);
    double entryZ = 2.5;
    int8_t intensityPercent = 90;
    PriceStrategy priceStrategy = PriceStrategy::nibble;
  };

  explicit MeanReversionMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  MeanReversionMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                            const Parameters &parameters) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  Parameters _parameters;
  TimePoint _lastDecisionTime;
  // Current exposure side from the last action: 0 = initial, +1 = heavy base (last action was a buy),
  // -1 = heavy quote (last action was a sell). Prevents repeated entries on the same side.
  int8_t _side{0};
};

}  // namespace cct
