#pragma once

#include <functional>
#include <unordered_map>

#include "cct_allocator.hpp"
#include "cct_flatset.hpp"
#include "cct_smallvector.hpp"
#include "currencycode.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"

namespace cct {

using MarketSet = FlatSet<Market, std::less<Market>, allocator<Market>, MarketVector>;
using MarketOrderBookMap = std::unordered_map<Market, MarketOrderBook>;
using MarketPriceMap = std::unordered_map<Market, MonetaryAmount>;
using MarketsPath = SmallVector<Market, 3>;

}  // namespace cct
