#pragma once

#include <chrono>

namespace cct {
/// Alias some types to make it easier to use
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Duration = Clock::duration;

using TimeInS = std::chrono::seconds;
using TimeInMs = std::chrono::milliseconds;
using TimeInUs = std::chrono::microseconds;

template <class T>
T GetTimeDiff(TimePoint t1, TimePoint t2) {
  return std::chrono::duration_cast<T>(t2 - t1);
}

template <class T>
T GetTimeFrom(TimePoint t1) {
  return GetTimeDiff<T>(t1, Clock::now());
}
}  // namespace cct
