#pragma once

#include <chrono>
#include <string>
#include <string_view>

namespace cct {
namespace api {
class TradeOptions {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  static constexpr Clock::duration kDefaultTradeDuration = std::chrono::seconds(30);
  static constexpr Clock::duration kDefaultEmergencyTime = std::chrono::milliseconds(2500);
  static constexpr Clock::duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  enum class Strategy {
    kMaker,           // Buy / sell at limit price for better conversion rate. Can be longer though.
    kMakerThenTaker,  // Start trade at limit price, updates the price to market price if at timeout order is not
                      // fully executed
    kTaker            // Take all available amount in the order book directly. Useful for arbitrage.
  };

  enum class Mode {
    kSimulation,
    kReal
  };  // An enum for documentation and compile time checking for such important option

  explicit TradeOptions(Strategy strategy = Strategy::kMaker, Clock::duration dur = kDefaultTradeDuration,
                        Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
                        Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates)
      : TradeOptions(strategy, Mode::kReal, dur, emergencyBufferTime, minTimeBetweenPriceUpdates) {}

  TradeOptions(Strategy strategy, Mode mode, Clock::duration dur,
               Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
               Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates);

  TradeOptions(std::string_view strategyStr, Mode mode, Clock::duration dur,
               Clock::duration emergencyBufferTime = kDefaultEmergencyTime,
               Clock::duration minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates);

  Clock::duration maxTradeTime() const { return _maxTradeTime; }

  Clock::duration emergencyBufferTime() const { return _emergencyBufferTime; }

  Clock::duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  Strategy strategy() const { return _strategy; }

  bool isTakerStrategy() const { return _strategy == Strategy::kTaker; }

  bool simulation() const { return _simulationMode; }

  std::string strategyStr() const;

  std::string str() const;

 private:
  Clock::duration _maxTradeTime;
  Clock::duration _emergencyBufferTime;
  Clock::duration _minTimeBetweenPriceUpdates;
  Strategy _strategy;
  bool _simulationMode;
};
}  // namespace api
}  // namespace cct