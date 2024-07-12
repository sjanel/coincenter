#pragma once

#include <map>

#include "market-auto-trade-options.hpp"
#include "market.hpp"

namespace cct {

using PublicExchangeAutoTradeOptions = std::map<Market, MarketAutoTradeOptions>;

}