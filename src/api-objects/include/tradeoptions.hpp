#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "priceoptions.hpp"
#include "timedef.hpp"
#include "tradedefinitions.hpp"

namespace cct {
class ExchangeConfig;
class TradeOptions {
 public:
  static constexpr Duration kDefaultMinTimeBetweenPriceUpdates = seconds(5);

  constexpr TradeOptions() noexcept = default;

  constexpr explicit TradeOptions(const PriceOptions &priceOptions) : _priceOptions(priceOptions) {}

  constexpr explicit TradeOptions(TradeMode tradeMode) : _tradeMode(tradeMode) {}

  /// Constructs a TradeOptions based on a continuously updated price from given string representation of trade
  /// strategy
  TradeOptions(const PriceOptions &priceOptions, TradeTimeoutAction timeoutAction, TradeMode tradeMode,
               Duration maxTradeTime, Duration minTimeBetweenPriceUpdates = kUndefinedDuration,
               TradeTypePolicy tradeTypePolicy = TradeTypePolicy::kDefault,
               TradeSyncPolicy tradeSyncPolicy = TradeSyncPolicy::kSynchronous);

  /// Constructs a new TradeOptions based on 'rhs' with unspecified options overriden from exchange config values
  TradeOptions(const TradeOptions &rhs, const ExchangeConfig &exchangeConfig);

  constexpr Duration maxTradeTime() const { return _maxTradeTime; }

  constexpr Duration minTimeBetweenPriceUpdates() const { return _minTimeBetweenPriceUpdates; }

  const PriceOptions &priceOptions() const { return _priceOptions; }

  constexpr PriceStrategy priceStrategy() const { return _priceOptions.priceStrategy(); }

  constexpr MonetaryAmount fixedPrice() const { return _priceOptions.fixedPrice(); }

  constexpr int relativePrice() const { return _priceOptions.relativePrice(); }

  constexpr TradeMode tradeMode() const { return _tradeMode; }

  constexpr TradeSyncPolicy tradeSyncPolicy() const { return _tradeSyncPolicy; }

  bool isMultiTradeAllowed(bool multiTradeAllowedByDefault) const;

  constexpr bool isTakerStrategy(bool placeRealOrderInSimulationMode) const {
    return _priceOptions.isTakerStrategy() && (!isSimulation() || !placeRealOrderInSimulationMode);
  }

  constexpr bool isSimulation() const { return _tradeMode == TradeMode::kSimulation; }

  constexpr bool isFixedPrice() const { return _priceOptions.isFixedPrice(); }

  constexpr bool isRelativePrice() const { return _priceOptions.isRelativePrice(); }

  constexpr bool placeMarketOrderAtTimeout() const { return _timeoutAction == TradeTimeoutAction::kMatch; }

  constexpr void switchToTakerStrategy() { _priceOptions.switchToTakerStrategy(); }

  std::string_view timeoutActionStr() const;

  std::string_view tradeSyncPolicyStr() const;

  string str(bool placeRealOrderInSimulationMode) const;

  bool operator==(const TradeOptions &) const noexcept = default;

 private:
  Duration _maxTradeTime = kUndefinedDuration;
  Duration _minTimeBetweenPriceUpdates = kUndefinedDuration;
  PriceOptions _priceOptions;
  TradeTimeoutAction _timeoutAction = TradeTimeoutAction::kDefault;
  TradeMode _tradeMode = TradeMode::kReal;
  TradeTypePolicy _tradeTypePolicy = TradeTypePolicy::kDefault;
  TradeSyncPolicy _tradeSyncPolicy = TradeSyncPolicy::kSynchronous;
};

}  // namespace cct