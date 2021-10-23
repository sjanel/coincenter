#pragma once

#include <chrono>
#include <string_view>

#include "cct_string.hpp"

namespace cct {
namespace api {

enum class TradeStrategy : int8_t {
  kMaker,           // Buy / sell at limit price for better conversion rate. Can be longer though.
  kMakerThenTaker,  // Start trade at limit price, updates the price to market price if at timeout order is not
                    // fully executed
  kTaker            // Take all available amount in the order book directly. Useful for arbitrage.
};

enum class TradeMode : int8_t {
  kSimulation,  // No real trade will be made. Useful for tests.
  kReal         // Real trade that will be executed in the exchange
};

enum class TradeType : int8_t {
  kSingleTrade,  // Single, 'fast' trade from 'startAmount' into 'toCurrency', on exchange named 'privateExchangeName'.
                 // 'fast' means that no unnecessary checks are done prior to the trade query, but if trade is
                 // impossible exception will be thrown
  kMultiTradePossible  // A Multi trade is similar to a single trade, at the difference that it retrieves the fastest
                       // currency conversion path and will launch several 'single' trades to reach that final goal.
                       // Example:
                       //  - Convert XRP to XLM on an exchange only proposing XRP-BTC and BTC-XLM markets will make 2
                       //  trades on these
                       //    markets.
};

class TradeOptions {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  static constexpr Clock::duration kDefaultTradeDuration = std::chrono::seconds(30);
  static constexpr Clock::duration kDefaultEmergencyTime = std::chrono::seconds(2);
  static constexpr Clock::duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  constexpr TradeOptions() noexcept = default;

  constexpr explicit TradeOptions(TradeStrategy tradeStrategy) : _strategy(tradeStrategy) {}

  constexpr explicit TradeOptions(TradeMode tradeMode) : _tradeMode(tradeMode) {}

  constexpr TradeOptions(TradeStrategy tradeStrategy, TradeMode tradeMode, Clock::duration dur,
                         Clock::duration emergencyBufferTime, Clock::duration minTimeBetweenPriceUpdates,
                         TradeType tradeType)
      : _maxTradeTime(dur),
        _emergencyBufferTime(emergencyBufferTime),
        _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
        _strategy(tradeStrategy),
        _tradeMode(tradeMode),
        _tradeType(tradeType) {}

  TradeOptions(std::string_view strategyStr, TradeMode tradeMode, Clock::duration dur,
               Clock::duration emergencyBufferTime, Clock::duration minTimeBetweenPriceUpdates, TradeType tradeType);

  constexpr Clock::duration maxTradeTime() const { return _maxTradeTime; }

  constexpr Clock::duration emergencyBufferTime() const { return _emergencyBufferTime; }

  constexpr Clock::duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  constexpr TradeStrategy strategy() const { return _strategy; }

  constexpr TradeMode tradeMode() const { return _tradeMode; }

  constexpr bool isMultiTradeAllowed() const { return _tradeType == TradeType::kMultiTradePossible; }

  constexpr bool isTakerStrategy() const { return _strategy == TradeStrategy::kTaker; }

  constexpr bool isSimulation() const { return _tradeMode == TradeMode::kSimulation; }

  constexpr void switchToTakerStrategy() { _strategy = TradeStrategy::kTaker; }

  std::string_view strategyStr() const;

  string str() const;

 private:
  Clock::duration _maxTradeTime = kDefaultTradeDuration;
  Clock::duration _emergencyBufferTime = kDefaultEmergencyTime;
  Clock::duration _minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates;
  TradeStrategy _strategy = TradeStrategy::kMaker;
  TradeMode _tradeMode = TradeMode::kReal;
  TradeType _tradeType = TradeType::kMultiTradePossible;
};
}  // namespace api
}  // namespace cct