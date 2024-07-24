#pragma once

#include <cstdint>

#include "time-window.hpp"

namespace cct {

struct TradeRangeResultsStats {
  TradeRangeResultsStats operator+(const TradeRangeResultsStats &rhs) const {
    return {timeWindow.aggregateMinMax(rhs.timeWindow), nbSuccessful + rhs.nbSuccessful, nbError + rhs.nbError};
  }

  TradeRangeResultsStats &operator+=(const TradeRangeResultsStats &rhs) { return *this = *this + rhs; }

  TimeWindow timeWindow;
  int32_t nbSuccessful{};
  int32_t nbError{};
};

struct TradeRangeStats {
  TradeRangeStats operator+(const TradeRangeStats &rhs) const {
    return {marketOrderBookStats + rhs.marketOrderBookStats, publicTradeStats + rhs.publicTradeStats};
  }

  TradeRangeStats &operator+=(const TradeRangeStats &rhs) { return *this = *this + rhs; }

  TradeRangeResultsStats marketOrderBookStats;
  TradeRangeResultsStats publicTradeStats;
};

}  // namespace cct