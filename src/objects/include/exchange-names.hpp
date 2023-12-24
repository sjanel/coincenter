#pragma once

#include <span>

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "exchangename.hpp"

namespace cct {

using ExchangeNameSpan = std::span<const ExchangeName>;
using ExchangeNames = SmallVector<ExchangeName, 1>;

using PublicExchangeNameVector = FixedCapacityVector<ExchangeName, kNbSupportedExchanges>;

string ConstructAccumulatedExchangeNames(ExchangeNameSpan exchangeNames);

}  // namespace cct