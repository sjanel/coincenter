#pragma once

#include <string_view>

#include "abstract-market-trader.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

/// Baseline reference algorithm: buys base with all available quote on the very first turn, then holds.
/// Useful to compare any active strategy against a simple "buy & hold" of the base currency.
class BuyAndHoldMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "buy-and-hold";

  explicit BuyAndHoldMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  bool _hasBought{false};
};

}  // namespace cct
