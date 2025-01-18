#pragma once

#include <span>

#include "cct_const.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "exchangename.hpp"

namespace cct {

using ExchangeNameSpan = std::span<const ExchangeName>;
using ExchangeNameEnumSpan = std::span<const ExchangeNameEnum>;
using ExchangeNames = SmallVector<ExchangeName, 1>;

using ExchangeNameEnumVector = FixedCapacityVector<ExchangeNameEnum, kNbSupportedExchanges>;

string ConstructAccumulatedExchangeNames(ExchangeNameSpan exchangeNames);

string ConstructAccumulatedExchangeNames(ExchangeNameEnumSpan exchangeNameEnums);

}  // namespace cct