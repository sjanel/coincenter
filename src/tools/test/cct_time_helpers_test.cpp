#include "cct_time_helpers.hpp"

#include <gtest/gtest.h>
#include <unistd.h>

namespace cct {
namespace time {

TEST(TimeHelper, Basic) {
  TimePoint t1 = GetTimePoint();
  usleep(1000);
  TimePoint t2 = GetTimePoint();
  TimeInUs res = GetTimeDiff<TimeInUs>(t1, t2);
  EXPECT_GE(res.count(), 1000);
  usleep(1000);
  res = GetTimeFrom<TimeInUs>(t1);
  EXPECT_GE(res.count(), 2000);
}

}  // namespace time
}  // namespace cct