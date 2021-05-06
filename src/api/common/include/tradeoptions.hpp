#pragma once

#include <chrono>
#include <string>
#include <string_view>

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
  static constexpr Clock::duration kDefaultEmergencyTime = std::chrono::milliseconds(2500);
  static constexpr Clock::duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  explicit TradeOptions(TradeStrategy tradeStrategy = TradeStrategy::kMaker,
                        Clock::duration dur = kDefaultTradeDuration,
                        Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
                        Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates)
      : TradeOptions(tradeStrategy, TradeMode::kReal, dur, emergencyBufferTime, minTimeBetweenPriceUpdates) {}

  TradeOptions(TradeStrategy tradeStrategy, TradeMode tradeMode, Clock::duration dur,
               Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
               Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates);

  TradeOptions(std::string_view strategyStr, TradeMode tradeMode, Clock::duration dur,
               Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
               Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates);

  Clock::duration maxTradeTime() const { return _maxTradeTime; }

  Clock::duration emergencyBufferTime() const { return _emergencyBufferTime; }

  Clock::duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  TradeStrategy strategy() const { return _strategy; }

  TradeMode tradeMode() const { return _tradeMode; }

  bool isTakerStrategy() const { return _strategy == TradeStrategy::kTaker; }

  bool isSimulation() const { return _tradeMode == TradeMode::kSimulation; }

  void switchToTakerStrategy() { _strategy = TradeStrategy::kTaker; }

  std::string strategyStr() const;

  std::string str() const;

 private:
  Clock::duration _maxTradeTime;
  Clock::duration _emergencyBufferTime;
  Clock::duration _minTimeBetweenPriceUpdates;
  TradeStrategy _strategy;
  TradeMode _tradeMode;
};
}  // namespace api
}  // namespace cct