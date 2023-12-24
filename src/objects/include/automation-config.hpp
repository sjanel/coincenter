#pragma once

#include <chrono>

#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class AutomationConfig {
 public:
  AutomationConfig() noexcept = default;

  AutomationConfig(Duration loadChunkDuration, MonetaryAmount startBaseAmountEquivalent,
                   MonetaryAmount startQuoteAmountEquivalent)
      : _loadChunkDuration(loadChunkDuration),
        _startBaseAmountEquivalent(startBaseAmountEquivalent),
        _startQuoteAmountEquivalent(startQuoteAmountEquivalent) {}

  Duration loadChunkDuration() const { return _loadChunkDuration; }

  MonetaryAmount startBaseAmountEquivalent() const { return _startBaseAmountEquivalent; }

  MonetaryAmount startQuoteAmountEquivalent() const { return _startQuoteAmountEquivalent; }

 private:
  Duration _loadChunkDuration = std::chrono::weeks(1);
  MonetaryAmount _startBaseAmountEquivalent;
  MonetaryAmount _startQuoteAmountEquivalent;
};
}  // namespace cct