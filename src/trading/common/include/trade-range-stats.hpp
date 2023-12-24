#pragma once

#include <cstdint>

namespace cct {

struct TradeRangeResultsStats {
  int32_t nbSuccessful{};
  int32_t nbError{};

  TradeRangeResultsStats operator+(const TradeRangeResultsStats &rhs) const {
    return TradeRangeResultsStats{nbSuccessful + rhs.nbSuccessful, nbError + rhs.nbError};
  }

  TradeRangeResultsStats &operator+=(const TradeRangeResultsStats &rhs) { return *this = *this + rhs; }
};

struct TradeRangeStats {
  TradeRangeResultsStats marketOrderBookStats;
  TradeRangeResultsStats publicTradeStats;

  TradeRangeStats operator+(const TradeRangeStats &rhs) const {
    return TradeRangeStats{marketOrderBookStats + rhs.marketOrderBookStats, publicTradeStats + rhs.publicTradeStats};
  }

  TradeRangeStats &operator+=(const TradeRangeStats &rhs) { return *this = *this + rhs; }
};

}  // namespace cct