#pragma once

#include <chrono>
#include <string_view>

#include "cct_string.hpp"

namespace cct {
namespace api {

enum class TradeStrategy {
  kMaker,           // Buy / sell at limit price for better conversion rate. Can be longer though.
  kMakerThenTaker,  // Start trade at limit price, updates the price to market price if at timeout order is not
                    // fully executed
  kTaker            // Take all available amount in the order book directly. Useful for arbitrage.
};

enum class TradeMode {
  kSimulation,
  kReal
};  // An enum for documentation and compile time checking for such important option

class TradeOptions {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  static constexpr Clock::duration kDefaultTradeDuration = std::chrono::seconds(30);
  static constexpr Clock::duration kDefaultEmergencyTime = std::chrono::seconds(2);
  static constexpr Clock::duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  constexpr explicit TradeOptions(TradeStrategy tradeStrategy = TradeStrategy::kMaker,
                                  Clock::duration dur = kDefaultTradeDuration,
                                  Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
                                  Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates)
      : TradeOptions(tradeStrategy, TradeMode::kReal, dur, emergencyBufferTime, minTimeBetweenPriceUpdates) {}

  constexpr TradeOptions(TradeStrategy tradeStrategy, TradeMode tradeMode, Clock::duration dur,
                         Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
                         Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates)
      : _maxTradeTime(dur),
        _emergencyBufferTime(emergencyBufferTime),
        _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
        _strategy(tradeStrategy),
        _tradeMode(tradeMode) {}

  TradeOptions(std::string_view strategyStr, TradeMode tradeMode, Clock::duration dur,
               Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
               Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates);

  constexpr Clock::duration maxTradeTime() const { return _maxTradeTime; }

  constexpr Clock::duration emergencyBufferTime() const { return _emergencyBufferTime; }

  constexpr Clock::duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  constexpr TradeStrategy strategy() const { return _strategy; }

  constexpr TradeMode tradeMode() const { return _tradeMode; }

  constexpr bool isTakerStrategy() const { return _strategy == TradeStrategy::kTaker; }

  constexpr bool isSimulation() const { return _tradeMode == TradeMode::kSimulation; }

  constexpr void switchToTakerStrategy() { _strategy = TradeStrategy::kTaker; }

  std::string_view strategyStr() const;

  string str() const;

 private:
  Clock::duration _maxTradeTime;
  Clock::duration _emergencyBufferTime;
  Clock::duration _minTimeBetweenPriceUpdates;
  TradeStrategy _strategy;
  TradeMode _tradeMode;
};
}  // namespace api
}  // namespace cct