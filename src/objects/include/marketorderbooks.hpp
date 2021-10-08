#pragma once

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "marketorderbook.hpp"

namespace cct {
using MarketOrderBooks = FixedCapacityVector<MarketOrderBook, kNbSupportedExchanges>;
}
