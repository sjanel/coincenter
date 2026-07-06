#pragma once

#include <cstdint>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

/// Simple spread-capturing market maker (ping-pong).
/// One command is allowed per turn, so it alternates sides based on the current inventory: when no order
/// is resting, it posts a maker buy at the bid while holding mostly quote, or a maker sell at the ask
/// while holding mostly base. As resting orders get filled, inventory flips and the side alternates,
/// capturing the bid/ask spread when the price oscillates.
/// NOTE: the simulation fills maker orders without queue-position modeling, so backtested results of this
/// family are optimistic and must be confirmed with live paper-trading.
class MarketMakerMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "market-maker";

  struct Parameters {
    int8_t intensityPercent = 50;
  };

  explicit MarketMakerMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  MarketMakerMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                          const Parameters &parameters) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  Parameters _parameters;
};

}  // namespace cct
