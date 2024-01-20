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

using TimeInS = std::chrono::seconds;
using TimeInMs = std::chrono::milliseconds;
using TimeInUs = std::chrono::microseconds;

template <class T>
constexpr T GetTimeDiff(TimePoint tp1, TimePoint tp2) {
  return std::chrono::duration_cast<T>(tp2 - tp1);
}

template <class T>
constexpr T GetTimeFrom(TimePoint tp) {
  return GetTimeDiff<T>(tp, Clock::now());
}

constexpr int64_t TimestampToS(TimePoint tp) {
  return std::chrono::duration_cast<TimeInS>(tp.time_since_epoch()).count();
}
constexpr int64_t TimestampToMs(TimePoint tp) {
  return std::chrono::duration_cast<TimeInMs>(tp.time_since_epoch()).count();
}
constexpr int64_t TimestampToUs(TimePoint tp) {
  return std::chrono::duration_cast<TimeInUs>(tp.time_since_epoch()).count();
}
}  // namespace cct
