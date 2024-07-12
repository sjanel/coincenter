#include "basic-stats.hpp"

#include <cmath>

#include "currencycode.hpp"
#include "market-data-view.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "publictrade.hpp"
#include "timedef.hpp"

namespace cct {

MonetaryAmount BasicStats::movingAverageFromLastPublicTradesPrice(TimePoint oldestTime) const {
  MonetaryAmount totalWeightedPrice;
  MonetaryAmount totalVolume;

  const auto lastPublicTrades = _marketDataView.pastPublicTrades();

  for (auto publicTradeIt = lastPublicTrades.end(); publicTradeIt != lastPublicTrades.begin();) {
    const PublicTrade &publicTrade = *(--publicTradeIt);

    if (publicTrade.time() < oldestTime) {
      break;
    }

    totalWeightedPrice += publicTrade.price() * publicTrade.amount();
    totalVolume += publicTrade.amount();
  }

  if (totalVolume == 0) {
    return totalWeightedPrice;
  }

  return totalWeightedPrice / totalVolume.toNeutral();
}

MonetaryAmount BasicStats::movingAverageFromMarketOrderBooks(TimePoint oldestTime,
                                                             Duration minFrequencyBetweenTwoPoints) const {
  MonetaryAmount totalPrice;

  const auto lastOrderBooks = _marketDataView.pastMarketOrderBooks();

  auto orderBookIt = lastOrderBooks.end();

  int nbPoints{};

  TimePoint previousTime = TimePoint::max();

  while (orderBookIt != lastOrderBooks.begin()) {
    const MarketOrderBook &marketOrderBook = *(--orderBookIt);
    const auto ts = marketOrderBook.time();

    if (ts < oldestTime) {
      break;
    }

    if (previousTime < ts + minFrequencyBetweenTwoPoints) {
      continue;
    }

    previousTime = ts;

    const auto optPrice = marketOrderBook.averagePrice();

    if (!optPrice) {
      continue;
    }

    totalPrice += *optPrice;
    ++nbPoints;
  }

  if (nbPoints == 0) {
    return totalPrice;
  }

  return totalPrice / nbPoints;
}

MonetaryAmount BasicStats::standardDeviationFromMarketOrderBooks(TimePoint oldestTime,
                                                                 Duration minFrequencyBetweenTwoPoints) const {
  double average = movingAverageFromMarketOrderBooks(oldestTime).toDouble();

  double squareDiffsSum{};

  const auto lastOrderBooks = _marketDataView.pastMarketOrderBooks();

  if (lastOrderBooks.empty()) {
    return MonetaryAmount{};
  }

  CurrencyCode priceCur = lastOrderBooks.back().market().quote();

  auto orderBookIt = lastOrderBooks.end();

  int nbPoints{};

  TimePoint previousTime = TimePoint::max();

  while (orderBookIt != lastOrderBooks.begin()) {
    const MarketOrderBook &marketOrderBook = *(--orderBookIt);
    const auto ts = marketOrderBook.time();

    if (ts < oldestTime) {
      break;
    }

    if (previousTime < ts + minFrequencyBetweenTwoPoints) {
      continue;
    }

    previousTime = ts;

    const auto optPrice = marketOrderBook.averagePrice();

    if (!optPrice) {
      continue;
    }

    const auto diff = average - optPrice->toDouble();

    squareDiffsSum += diff * diff;
    ++nbPoints;
  }

  if (nbPoints == 0) {
    return MonetaryAmount{0, priceCur};
  }

  // Population standard deviation: sqrt(mean of squared deviations) - the sum must be divided by the
  // number of points before taking the square root.
  return MonetaryAmount{std::sqrt(squareDiffsSum / nbPoints), priceCur};
}

double BasicStats::relativeStrengthIndexFromMarketOrderBooks(TimePoint oldestTime, Duration samplingPeriod) const {
  const auto lastOrderBooks = _marketDataView.pastMarketOrderBooks();

  double sumGain = 0.0;
  double sumLoss = 0.0;
  int nbDeltas = 0;

  // Walk newest -> oldest, keeping at most one sample per 'samplingPeriod' bucket, and accumulate the
  // gains / losses between consecutive samples.
  bool hasNewer = false;
  double newerPrice = 0.0;
  TimePoint lastSampledTime = TimePoint::max();

  for (auto it = lastOrderBooks.end(); it != lastOrderBooks.begin();) {
    const MarketOrderBook &marketOrderBook = *(--it);
    const auto ts = marketOrderBook.time();

    if (ts < oldestTime) {
      break;
    }
    if (lastSampledTime != TimePoint::max() && lastSampledTime - ts < samplingPeriod) {
      continue;
    }

    const auto optPrice = marketOrderBook.averagePrice();
    if (!optPrice) {
      continue;
    }
    const double price = optPrice->toDouble();

    if (hasNewer) {
      const double delta = newerPrice - price;  // change from this (older) sample to the newer one
      if (delta > 0.0) {
        sumGain += delta;
      } else {
        sumLoss += -delta;
      }
      ++nbDeltas;
    }

    newerPrice = price;
    hasNewer = true;
    lastSampledTime = ts;
  }

  if (nbDeltas == 0) {
    return -1.0;  // not enough data
  }

  const double avgGain = sumGain / nbDeltas;
  const double avgLoss = sumLoss / nbDeltas;

  if (avgLoss == 0.0) {
    return 100.0;
  }

  const double rs = avgGain / avgLoss;
  return 100.0 - (100.0 / (1.0 + rs));
}

}  // namespace cct