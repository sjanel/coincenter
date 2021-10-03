#pragma once

#include "exchange.hpp"
#include "exchangeretrieverbase.hpp"

namespace cct {
using ExchangeRetriever = ExchangeRetrieverBase<Exchange>;
using ConstExchangeRetriever = ExchangeRetrieverBase<const Exchange>;
}  // namespace cct