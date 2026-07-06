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

/// RSI oscillator trader (short-term mean-reversion).
/// Buys base when the RSI falls into oversold territory and sells base when it rises into overbought
/// territory. Uses hysteresis (one action per crossing, alternating sides) plus a decision cadence so the
/// trade frequency - and therefore the fee drag - stays low. Defaults to maker orders to minimize fees.
class RsiMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "rsi";

  struct Parameters {
    // A longer sampling period + matching decision cadence keep the RSI smooth and the trade count low;
    // short-term RSI on dense crypto data over-trades and is destroyed by fees.
    int rsiPeriods = 14;
    Duration samplingPeriod = std::chrono::minutes(15);
    Duration decisionInterval = std::chrono::minutes(15);
    double oversold = 30.0;
    double overbought = 70.0;
    int8_t intensityPercent = 90;
    PriceStrategy priceStrategy = PriceStrategy::maker;
  };

  explicit RsiMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  RsiMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                  const Parameters &parameters) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  Parameters _parameters;
  TimePoint _lastDecisionTime;
  // 0 = initial, +1 = heavy base (last action was a buy), -1 = heavy quote (last action was a sell).
  int8_t _side{0};
};

}  // namespace cct
