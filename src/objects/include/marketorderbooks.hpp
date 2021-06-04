#pragma once

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "marketorderbook.hpp"

namespace cct {
using MarketOrderBooks = cct::FixedCapacityVector<MarketOrderBook, kNbSupportedExchanges>;
}
