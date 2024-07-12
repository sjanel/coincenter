#pragma once

#include <cstdint>
#include <string_view>

#include "abstract-market-trader.hpp"
#include "priceoptionsdef.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketDataView;
class MarketTraderEngineState;

/// Grid / DCA trader.
/// Keeps a moving anchor price. Each time the mid price drops one grid step below the anchor it buys a
/// slice of base (buying dips), each time it rises one grid step above it sells a slice (selling rips),
/// re-anchoring at the new price. Profits from oscillating / ranging markets; accumulates inventory in
/// strong down-trends (bounded by the available balance).
class GridMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "grid";

  struct Parameters {
    double stepRatio = 0.01;  // 1% grid spacing between two levels
    int8_t intensityPercent = 20;
    PriceStrategy priceStrategy = PriceStrategy::nibble;
  };

  explicit GridMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  GridMarketTrader(std::string_view name, const MarketTraderEngineState &marketTraderEngineState,
                   const Parameters &parameters) noexcept;

  TraderCommand trade(const MarketDataView &marketDataView) override;

 private:
  Parameters _parameters;
  double _anchorPrice{0.0};
};

}  // namespace cct
