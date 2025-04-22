#pragma once

#include <chrono>
#include <cstdint>
#include <string_view>

#include "cct_format.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "generic-object-json.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {

/// Simple utility class representing a time window with a beginning time, and an end time.
/// The beginning time includes the corresponding time point, but the end time excludes it.
class TimeWindow {
 public:
  static constexpr auto kTimeFormat = kTimeYearToSecondTSeparatedFormatUTC;

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

  /// Create a time window from a string representation.
  explicit TimeWindow(std::string_view timeWindowStr);

  TimePoint from() const { return _from; }

  TimePoint to() const { return _to; }

  Duration duration() const { return _to - _from; }

  bool contains(TimePoint tp) const { return _from <= tp && tp < _to; }

  bool contains(int64_t unixTimestampInMs) const { return contains(TimePoint(milliseconds{unixTimestampInMs})); }

  bool contains(TimeWindow rhs) const { return _from <= rhs._from && rhs._to <= _to; }

  bool overlaps(TimeWindow rhs) const { return _from < rhs._to && rhs._from < _to; }

  /// Returns a new time window with maximum boundaries of both.
  TimeWindow aggregateMinMax(TimeWindow rhs) const;

  TimeWindow operator+(Duration dur) const { return TimeWindow(_from + dur, _to + dur); }

  TimeWindow &operator+=(Duration dur) { return *this = *this + dur; }

  string str() const;

  char *appendTo(char *buf) const;

  static constexpr size_t strLen() { return kTimeWindowLen; }

  bool operator==(const TimeWindow &) const noexcept = default;

 private:
  static constexpr std::string_view kArrow = " -> ";
  static constexpr size_t kYNbChars = 4;
  static constexpr size_t kMonthNbChars = 2;
  static constexpr size_t kDayNbChars = 2;
  static constexpr size_t kHourNbChars = 2;
  static constexpr size_t kMinuteNbChars = 2;
  static constexpr size_t kSecondNbChars = 2;

  static constexpr size_t kTimeNbChars = kYNbChars + 1U + kMonthNbChars + 1U + kDayNbChars + 1U + kHourNbChars + 1U +
                                         kMinuteNbChars + 1U + kSecondNbChars + 1U;

  static constexpr size_t kTimeWindowLen = 2U + 2U * kTimeNbChars + kArrow.size();

  TimePoint _from;
  TimePoint _to;
};
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::TimeWindow> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::TimeWindow &timeWindow, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", timeWindow.str());
  }
};
#endif

namespace glz {
template <>
struct from<JSON, ::cct::TimeWindow> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) noexcept {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value = ::cct::TimeWindow(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::TimeWindow> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    ::cct::details::ToStrLikeJson<Opts>(value, b, ix);
  }
};
}  // namespace glz
