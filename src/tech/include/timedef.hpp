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

using TimeInS = std::chrono::seconds;
using TimeInMs = std::chrono::milliseconds;
using TimeInUs = std::chrono::microseconds;

template <class T>
T GetTimeDiff(TimePoint t1, TimePoint t2) {
  return std::chrono::duration_cast<T>(t2 - t1);
}

template <class T>
T GetTimeFrom(TimePoint t) {
  return GetTimeDiff<T>(t, Clock::now());
}

constexpr int64_t TimestampToS(TimePoint t) {
  return std::chrono::duration_cast<TimeInS>(t.time_since_epoch()).count();
}
constexpr int64_t TimestampToMs(TimePoint t) {
  return std::chrono::duration_cast<TimeInMs>(t.time_since_epoch()).count();
}
constexpr int64_t TimestampToUs(TimePoint t) {
  return std::chrono::duration_cast<TimeInUs>(t.time_since_epoch()).count();
}
}  // namespace cct
