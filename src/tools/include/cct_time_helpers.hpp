#pragma once

#include <chrono>
#include <ctime>
#include <string>

namespace cct {
namespace time {
/// Alias some types to make it easier to use
using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;

using TimeInS = std::chrono::seconds;
using TimeInMs = std::chrono::milliseconds;
using TimeInUs = std::chrono::microseconds;

inline TimePoint GetTimePoint() { return Clock::now(); }

template <typename T>
T GetTimeDiff(TimePoint t1, TimePoint t2) {
  return std::chrono::duration_cast<T>(t2 - t1);
}

template <typename T>
T GetTimeFrom(TimePoint t1) {
  TimePoint t2 = GetTimePoint();
  return GetTimeDiff<T>(t1, t2);
}
}  // namespace time
}  // namespace cct
