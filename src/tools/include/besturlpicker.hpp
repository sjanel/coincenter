#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "cct_fixedcapacityvector.hpp"
#include "cct_string.hpp"

namespace cct {
/// Utility class that holds the logic to pick the most interesting URL for each query based on response time
/// statistics (average and standard deviation) stored over requests.
/// The maximum number of base URLs it can work with is known at compile time, and should stay small as data is stored
/// inline.
/// BestURLPicker basically favors the base URLs with the lowest average response time and deviation (scored as a sum,
/// so the average naturally counts 'more' than the deviation).
/// We approximate storage of a moving average of response time and standard deviation instead of holding stats for the
/// 'n' last requests.
class BestURLPicker {
 private:
  static constexpr int kNbMaxBaseUrl = 4;

 public:
  /// Builds a BestURLPicker that will work with several base URLs.
  /// Warning: given base URL should come from static storage
  template <unsigned N, std::enable_if_t<(N > 0) && N <= kNbMaxBaseUrl, bool> = true>
  BestURLPicker(const std::string_view (&aBaseUrl)[N]) : BestURLPicker(std::span<const std::string_view>(aBaseUrl)) {}

  BestURLPicker(const string[]) = delete;
  BestURLPicker(const char *[]) = delete;

  /// Builds a BestURLPicker with a single base URL.
  /// The chosen base URL is thus trivial and will always be the same.
  /// Warning: given base URL should come from static storage
  BestURLPicker(const std::string_view &singleBaseUrl)
      : BestURLPicker(std::span<const std::string_view>(std::addressof(singleBaseUrl), 1)) {}

  BestURLPicker(const string &) = delete;
  BestURLPicker(const char *) = delete;

  // Return the best URL that will be used by the next query.
  // A "good" URL is some URL that has lower average response time (all queries mixed) according to the others.
  std::string_view getNextBaseURL() const { return _pBaseUrls[nextBaseURLPos()]; }
  std::string_view getBaseURL(int8_t pos) const { return _pBaseUrls[pos]; }

  int8_t nextBaseURLPos() const;
  void storeResponseTimePerBaseURL(int8_t baseUrlPos, uint32_t responseTimeInMs);

  int8_t nbBaseURL() const { return static_cast<int8_t>(_responseTimeStatsPerBaseUrl.size()); }

 private:
  explicit BestURLPicker(std::span<const std::string_view> baseUrls)
      : _pBaseUrls(baseUrls.data()), _responseTimeStatsPerBaseUrl(baseUrls.size()) {}

  struct ResponseTimeStats {
    uint16_t nbRequestsDone;   // when reaching max, all stats are reset to give equal chances to all base URLs
    uint16_t avgResponseTime;  // approximation of moving average
    uint16_t avgDeviation;     // approximation of moving standard deviation

    bool operator==(const ResponseTimeStats &) const noexcept = default;

    uint32_t score() const { return static_cast<uint32_t>(avgResponseTime) + avgDeviation; }
  };

  using ResponseTimeStatsPerBaseUrl = FixedCapacityVector<ResponseTimeStats, kNbMaxBaseUrl>;

  // Non-owning pointer, should come from static storage (default special operations are fine)
  const std::string_view *_pBaseUrls;
  ResponseTimeStatsPerBaseUrl _responseTimeStatsPerBaseUrl;
};
}  // namespace cct