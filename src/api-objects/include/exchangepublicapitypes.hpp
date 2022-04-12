#pragma once

#include <unordered_map>

#include "cct_flatset.hpp"
#include "cct_smallvector.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "publictrade.hpp"

namespace cct {
using MarketSet = FlatSet<Market>;
using MarketOrderBookMap = std::unordered_map<Market, MarketOrderBook>;
using MarketPriceMap = std::unordered_map<Market, MonetaryAmount>;
using MarketsPath = SmallVector<Market, 3>;
using WithdrawalFeeMap = std::unordered_map<CurrencyCode, MonetaryAmount>;
using LastTradesVector = vector<PublicTrade>;
}  // namespace cct