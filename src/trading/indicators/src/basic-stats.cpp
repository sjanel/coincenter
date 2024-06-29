#include "basic-stats.hpp"

#include <cmath>

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

  return MonetaryAmount{std::sqrt(squareDiffsSum), priceCur};
}

}  // namespace cct