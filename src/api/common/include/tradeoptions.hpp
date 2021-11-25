#pragma once

#include <chrono>
#include <string_view>

#include "cct_string.hpp"
#include "tradedefinitions.hpp"

namespace cct {
class TradeOptions {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  static constexpr Clock::duration kDefaultTradeDuration = std::chrono::seconds(30);
  static constexpr Clock::duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  constexpr TradeOptions() noexcept = default;

  constexpr explicit TradeOptions(TradePriceStrategy tradeStrategy) : _priceStrategy(tradeStrategy) {}

  constexpr explicit TradeOptions(TradeMode tradeMode) : _mode(tradeMode) {}

  constexpr TradeOptions(TradePriceStrategy tradeStrategy, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
                         Clock::duration dur, Clock::duration minTimeBetweenPriceUpdates, TradeType tradeType)
      : _maxTradeTime(dur),
        _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
        _priceStrategy(tradeStrategy),
        _timeoutAction(timeoutAction),
        _mode(tradeMode),
        _type(tradeType) {}

  TradeOptions(std::string_view priceStrategyStr, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
               Clock::duration dur, Clock::duration minTimeBetweenPriceUpdates, TradeType tradeType);

  constexpr Clock::duration maxTradeTime() const { return _maxTradeTime; }

  constexpr Clock::duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  constexpr TradePriceStrategy priceStrategy() const { return _priceStrategy; }

  constexpr TradeMode tradeMode() const { return _mode; }

  constexpr bool isMultiTradeAllowed() const { return _type == TradeType::kMultiTradePossible; }

  constexpr bool isTakerStrategy() const { return _priceStrategy == TradePriceStrategy::kTaker; }

  constexpr bool isSimulation() const { return _mode == TradeMode::kSimulation; }

  constexpr bool placeMarketOrderAtTimeout() const { return _timeoutAction == TradeTimeoutAction::kForceMatch; }

  constexpr void switchToTakerStrategy() { _priceStrategy = TradePriceStrategy::kTaker; }

  std::string_view priceStrategyStr() const;

  std::string_view timeoutActionStr() const;

  string str() const;

 private:
  Clock::duration _maxTradeTime = kDefaultTradeDuration;
  Clock::duration _minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates;
  TradePriceStrategy _priceStrategy = TradePriceStrategy::kMaker;
  TradeTimeoutAction _timeoutAction = TradeTimeoutAction::kCancel;
  TradeMode _mode = TradeMode::kReal;
  TradeType _type = TradeType::kMultiTradePossible;
};
}  // namespace cct