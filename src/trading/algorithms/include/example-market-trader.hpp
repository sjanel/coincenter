#pragma once

#include <span>

#include "abstract-market-trader.hpp"
#include "market-trader-engine-state.hpp"
#include "marketorderbook.hpp"
#include "publictrade.hpp"
#include "trader-command.hpp"

namespace cct {
class ExampleMarketTrader : public AbstractMarketTrader {
 public:
  static constexpr std::string_view kName = "example-trader";

  ExampleMarketTrader(const MarketTraderEngineState &marketTraderEngineState) noexcept;

  TraderCommand trade([[maybe_unused]] const MarketOrderBook &marketOrderBook,
                      [[maybe_unused]] std::span<const PublicTrade> trades) override;
};
}  // namespace cct