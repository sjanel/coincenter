#pragma once

#include <chrono>
#include <cstdint>

namespace cct {

/// Alias some types to make it easier to use
/// The main clock is system_clock as it is the only one guaranteed to provide conversions to Unix epoch time.
/// It is not monotonic - thus for unit tests we will prefer usage of steady_clock
using Clock = std::chrono::system_clock;
using TimePoint = Clock::time_point;
using Duration = Clock::duration;

static constexpr auto kUndefinedDuration = Duration::min();

using seconds = std::chrono::seconds;
using milliseconds = std::chrono::milliseconds;
using microseconds = std::chrono::microseconds;

template <class T>
constexpr T GetTimeDiff(TimePoint tp1, TimePoint tp2) {
  return std::chrono::duration_cast<T>(tp2 - tp1);
}

template <class T>
constexpr T GetTimeFrom(TimePoint tp) {
  return GetTimeDiff<T>(tp, Clock::now());
}

constexpr int64_t TimestampToSecondsSinceEpoch(TimePoint tp) {
  return std::chrono::duration_cast<seconds>(tp.time_since_epoch()).count();
}

constexpr int64_t TimestampToMillisecondsSinceEpoch(TimePoint tp) {
  return std::chrono::duration_cast<milliseconds>(tp.time_since_epoch()).count();
}

constexpr int64_t TimestampToUs(TimePoint tp) {
  return std::chrono::duration_cast<microseconds>(tp.time_since_epoch()).count();
}

}  // namespace cct
