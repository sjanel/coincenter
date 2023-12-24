#include "abstract-market-trader.hpp"

#include <string_view>

#include "market-trader-engine-state.hpp"

namespace cct {

AbstractMarketTrader::AbstractMarketTrader(std::string_view name,
                                           const MarketTraderEngineState& marketTraderEngineState) noexcept
    : _name(name), _marketTraderEngineState(marketTraderEngineState) {}

}  // namespace cct