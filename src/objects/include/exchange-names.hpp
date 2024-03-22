#pragma once

#include <span>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "exchangename.hpp"

namespace cct {

using ExchangeNameSpan = std::span<const ExchangeName>;
using ExchangeNames = SmallVector<ExchangeName, 1>;

string ConstructAccumulatedExchangeNames(ExchangeNameSpan exchangeNames);

}  // namespace cct