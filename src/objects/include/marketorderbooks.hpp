#pragma once

#include "cct_const.hpp"
#include "cct_smallvector.hpp"
#include "marketorderbook.hpp"

namespace cct {
using MarketOrderBooks = cct::SmallVector<MarketOrderBook, kTypicalNbExchanges>;
}
