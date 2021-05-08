#include "cct_time_helpers.hpp"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

namespace cct {
namespace time {

TEST(TimeHelper, Basic) {
  TimePoint t1 = GetTimePoint();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  TimePoint t2 = GetTimePoint();
  TimeInUs res = GetTimeDiff<TimeInUs>(t1, t2);
  EXPECT_GE(res.count(), 1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  res = GetTimeFrom<TimeInUs>(t1);
  EXPECT_GE(res.count(), 2000);
}

}  // namespace time
}  // namespace cct