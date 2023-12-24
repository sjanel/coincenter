#pragma once

#include <chrono>
#include <cstdint>

#include "cct_format.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "timedef.hpp"

namespace cct {

/// Simple utility class representing a time window with a beginning time, and an end time.
/// The beginning time includes the corresponding time point, but the end time excludes it.
class TimeWindow {
 public:
  /// Create a zero duration time window starting from the zero-initialized time point.
  TimeWindow() noexcept = default;

  /// Create a time window spanning from 'from' (included) to 'to' (excluded) time points.
  TimeWindow(TimePoint from, TimePoint to) : _from(from), _to(to) {
    if (_to < _from) {
      throw invalid_argument("Invalid time window - 'from' should not be larger than 'to'");
    }
  }

  /// Create a time window starting at 'from' with 'dur' duration.
  TimeWindow(TimePoint from, Duration dur) : TimeWindow(from, from + dur) {}

  TimePoint from() const { return _from; }

  TimePoint to() const { return _to; }

  Duration duration() const { return _to - _from; }

  bool contains(TimePoint tp) const { return _from <= tp && tp < _to; }

  bool contains(int64_t unixTimestampInMs) const { return contains(TimePoint(milliseconds{unixTimestampInMs})); }

  bool contains(TimeWindow rhs) const { return _from <= rhs._from && rhs._to <= _to; }

  bool overlaps(TimeWindow rhs) const { return _from < rhs._to && rhs._from < _to; }

  string str() const;

  bool operator==(const TimeWindow&) const noexcept = default;

 private:
  TimePoint _from;
  TimePoint _to;
};
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::TimeWindow> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::TimeWindow& timeWindow, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", timeWindow.str());
  }
};
#endif
