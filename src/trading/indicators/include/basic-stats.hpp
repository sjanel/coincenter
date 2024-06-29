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

 private:
  const MarketDataView &_marketDataView;
};

}  // namespace cct