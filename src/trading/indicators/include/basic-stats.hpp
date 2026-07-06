#pragma once

#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {

class MarketDataView;

class BasicStats {
 public:
  explicit BasicStats(const MarketDataView &marketDataView) : _marketDataView(marketDataView) {}

  MonetaryAmount movingAverageFromLastPublicTradesPrice(TimePoint oldestTime) const;

  MonetaryAmount movingAverageFromMarketOrderBooks(TimePoint oldestTime,
                                                   Duration minFrequencyBetweenTwoPoints = Duration{}) const;

  MonetaryAmount standardDeviationFromMarketOrderBooks(TimePoint oldestTime,
                                                       Duration minFrequencyBetweenTwoPoints = Duration{}) const;

  /// Relative Strength Index (RSI) of the mid price, in [0, 100].
  /// The mid price is sampled once per 'samplingPeriod' bucket over [oldestTime, now]; consecutive sampled
  /// changes feed the average gain / average loss. Returns a negative value if there are not enough samples.
  double relativeStrengthIndexFromMarketOrderBooks(TimePoint oldestTime, Duration samplingPeriod) const;

 private:
  const MarketDataView &_marketDataView;
};

}  // namespace cct