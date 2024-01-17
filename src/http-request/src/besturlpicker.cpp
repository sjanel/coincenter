#include "besturlpicker.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>

#include "cct_log.hpp"
#include "mathhelpers.hpp"

namespace cct {
int8_t BestURLPicker::nextBaseURLPos() const {
  // First, pick the base url which has less than 'kNbRequestMinBeforeCompare' if any
  auto minNbIt = std::ranges::find_if(_responseTimeStatsPerBaseUrl, [](ResponseTimeStats lhs) {
    static constexpr decltype(lhs.nbRequestsDone) kNbRequestMinBeforeCompare = 10;
    return lhs.nbRequestsDone < kNbRequestMinBeforeCompare;
  });
  if (minNbIt != _responseTimeStatsPerBaseUrl.end()) {
    return static_cast<int8_t>(minNbIt - _responseTimeStatsPerBaseUrl.begin());
  }

  // Let's compute a 'score' based on the average deviation and the avg response time and pick best url
  // The lowest score will correspond to the best URL
  auto nextBaseURLIt =
      std::ranges::min_element(_responseTimeStatsPerBaseUrl,
                               [](ResponseTimeStats lhs, ResponseTimeStats rhs) { return lhs.score() < rhs.score(); });

  // We favor the URL that has the least score for 90 % of the requests, and give a chance to the one with the least
  // number of requests 10 % of the time, not counting the one with the best score.
  int totalNbRequestsDone = nbRequestsDone();
  if ((totalNbRequestsDone % 10) == 9) {
    ResponseTimeStats minScoreResponseTimeStats = *nextBaseURLIt;

    nextBaseURLIt = std::ranges::min_element(_responseTimeStatsPerBaseUrl,
                                             [minScoreResponseTimeStats](ResponseTimeStats lhs, ResponseTimeStats rhs) {
                                               if (lhs == minScoreResponseTimeStats) {
                                                 return false;
                                               }
                                               if (rhs == minScoreResponseTimeStats) {
                                                 return true;
                                               }
                                               return lhs.nbRequestsDone < rhs.nbRequestsDone;
                                             });
  }

  return static_cast<int8_t>(nextBaseURLIt - _responseTimeStatsPerBaseUrl.begin());
}

void BestURLPicker::storeResponseTimePerBaseURL(int8_t baseUrlPos, uint32_t responseTimeInMs) {
  ResponseTimeStats &stats = _responseTimeStatsPerBaseUrl[baseUrlPos];

  // How many requests we consider to compute stats?
  using NbRequestType = decltype(stats.nbRequestsDone);
  static constexpr NbRequestType kMaxLastNbRequestsToConsider = 20;

  if (stats.nbRequestsDone == std::numeric_limits<NbRequestType>::max()) {
    // If one URL has reached the max number of requests done, we reset all stats and give an equal chance for all Base
    // URLs once again
    log::debug("Reset time stats for '{}'", _pBaseUrls[baseUrlPos]);
    std::ranges::fill(_responseTimeStatsPerBaseUrl, ResponseTimeStats{});
    return;
  }

  ++stats.nbRequestsDone;

  NbRequestType nbRequestsToConsider = std::min(stats.nbRequestsDone, kMaxLastNbRequestsToConsider);

  // Update moving average
  uint64_t sumResponseTime = static_cast<uint64_t>(stats.avgResponseTimeInMs) * (nbRequestsToConsider - 1);
  sumResponseTime += responseTimeInMs;
  uint64_t newAverageResponseTime = sumResponseTime / nbRequestsToConsider;
  using RTType = decltype(stats.avgResponseTimeInMs);
  if (newAverageResponseTime > static_cast<uint64_t>(std::numeric_limits<RTType>::max())) {
    log::warn("Cannot update accurately the new average response time {} because of overflow", newAverageResponseTime);
    stats.avgResponseTimeInMs = std::numeric_limits<RTType>::max();
  } else {
    stats.avgResponseTimeInMs = static_cast<RTType>(newAverageResponseTime);
  }

  // Update moving deviation
  uint64_t sumDeviation = static_cast<uint64_t>(ipow(stats.avgDeviationInMs, 2)) * (nbRequestsToConsider - 1);
  sumDeviation += static_cast<uint64_t>(
      ipow(static_cast<int64_t>(stats.avgResponseTimeInMs) - static_cast<int64_t>(responseTimeInMs), 2));
  auto newDeviationResponseTime = static_cast<uint64_t>(std::sqrt(sumDeviation / nbRequestsToConsider));
  using DevType = decltype(stats.avgDeviationInMs);
  if (newDeviationResponseTime > static_cast<uint64_t>(std::numeric_limits<DevType>::max())) {
    log::warn("Cannot update accurately the new deviation response time {} because of overflow",
              newDeviationResponseTime);
    stats.avgDeviationInMs = std::numeric_limits<DevType>::max();
  } else {
    stats.avgDeviationInMs = static_cast<DevType>(newDeviationResponseTime);
  }

  log::debug("Response time stats for '{}': Avg: {} ms, Dev: {} ms, Nb: {} (last: {} ms)", _pBaseUrls[baseUrlPos],
             stats.avgResponseTimeInMs, stats.avgDeviationInMs, stats.nbRequestsDone, responseTimeInMs);
}

int BestURLPicker::nbRequestsDone() const {
  return std::accumulate(_responseTimeStatsPerBaseUrl.begin(), _responseTimeStatsPerBaseUrl.end(), 0,
                         [](int sum, ResponseTimeStats stats) { return sum + stats.nbRequestsDone; });
}

}  // namespace cct
