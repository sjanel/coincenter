#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timehelpers.hpp"
#include "tradedefinitions.hpp"

namespace cct {
class TradeOptions {
 public:
  static constexpr Duration kDefaultTradeDuration = std::chrono::seconds(30);
  static constexpr Duration kDefaultMinTimeBetweenPriceUpdates = std::chrono::seconds(5);

  constexpr TradeOptions() noexcept = default;

  constexpr explicit TradeOptions(TradePriceStrategy tradeStrategy) : _priceStrategy(tradeStrategy) {}

  constexpr explicit TradeOptions(TradeMode tradeMode) : _mode(tradeMode) {}

  TradeOptions(TradeTimeoutAction timeoutAction, TradeMode tradeMode, Duration dur, Duration minTimeBetweenPriceUpdates,
               TradeType tradeType)
      : _maxTradeTime(dur),
        _minTimeBetweenPriceUpdates(minTimeBetweenPriceUpdates),
        _timeoutAction(timeoutAction),
        _mode(tradeMode),
        _type(tradeType) {}

  /// Constructs a TradeOptions based on a continuously updated price from given string representation of trade
  /// strategy
  TradeOptions(std::string_view priceStrategyStr, TradeTimeoutAction timeoutAction, TradeMode tradeMode, Duration dur,
               Duration minTimeBetweenPriceUpdates, TradeType tradeType);

  /// Constructs a TradeOptions based on a fixed absolute price.
  /// Multi trade is not supported in this case.
  TradeOptions(MonetaryAmount fixedPrice, TradeTimeoutAction timeoutAction, TradeMode tradeMode, Duration dur);

  /// Constructs a TradeOptions based on a fixed relative price (relative from limit price).
  TradeOptions(TradeRelativePrice relativePrice, TradeTimeoutAction timeoutAction, TradeMode tradeMode, Duration dur,
               TradeType tradeType);

  constexpr Duration maxTradeTime() const { return _maxTradeTime; }

  constexpr Duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  constexpr TradePriceStrategy priceStrategy() const { return _priceStrategy; }

  constexpr MonetaryAmount fixedPrice() const { return _fixedPrice; }

  constexpr int relativePrice() const { return _relativePrice; }

  constexpr TradeMode tradeMode() const { return _mode; }

  constexpr bool isMultiTradeAllowed() const { return _type == TradeType::kMultiTradePossible; }

  constexpr bool isTakerStrategy(bool placeRealOrderInSimulationMode) const {
    return _priceStrategy == TradePriceStrategy::kTaker && (!isSimulation() || !placeRealOrderInSimulationMode);
  }

  constexpr bool isSimulation() const { return _mode == TradeMode::kSimulation; }

  constexpr bool isFixedPrice() const { return !_fixedPrice.isDefault(); }

  constexpr bool isRelativePrice() const { return _relativePrice != kTradeNoRelativePrice; }

  constexpr bool placeMarketOrderAtTimeout() const { return _timeoutAction == TradeTimeoutAction::kForceMatch; }

  constexpr void switchToTakerStrategy() { _priceStrategy = TradePriceStrategy::kTaker; }

  std::string_view timeoutActionStr() const;

  string str(bool placeRealOrderInSimulationMode) const;

 private:
  std::string_view priceStrategyStr(bool placeRealOrderInSimulationMode) const;

  Duration _maxTradeTime = kDefaultTradeDuration;
  Duration _minTimeBetweenPriceUpdates = kDefaultMinTimeBetweenPriceUpdates;
  MonetaryAmount _fixedPrice;
  TradeRelativePrice _relativePrice = kTradeNoRelativePrice;
  TradePriceStrategy _priceStrategy = TradePriceStrategy::kMaker;
  TradeTimeoutAction _timeoutAction = TradeTimeoutAction::kCancel;
  TradeMode _mode = TradeMode::kReal;
  TradeType _type = TradeType::kMultiTradePossible;
};
}  // namespace cct