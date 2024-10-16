#pragma once

#include <chrono>

#include "duration-schema.hpp"
#include "monetaryamount.hpp"

namespace cct::schema {

struct DeserializationConfig {
  Duration loadChunkDuration{std::chrono::weeks(1)};
};

struct StartingContextConfig {
  MonetaryAmount startBaseAmountEquivalent{1000, "EUR"};
  MonetaryAmount startQuoteAmountEquivalent{1000, "EUR"};
};

struct AutomationConfig {
  DeserializationConfig deserialization;
  StartingContextConfig startingContext;
};

struct TradingConfig {
  AutomationConfig automation;
};

}  // namespace cct::schema