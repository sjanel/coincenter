#include "timedef.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <thread>

namespace cct {

TEST(TimeDefinitions, Basic) {
  TimePoint t1 = Clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  TimePoint t2 = Clock::now();
  TimeInUs res = GetTimeDiff<TimeInUs>(t1, t2);
  EXPECT_GE(res.count(), 1000);
  std::this_thread::sleep_for(std::chrono::milliseconds(1));
  res = GetTimeFrom<TimeInUs>(t1);
  EXPECT_GE(res.count(), 2000);
}

}  // namespace cct