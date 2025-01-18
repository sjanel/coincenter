#include "time-window.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "timedef.hpp"

namespace cct {
class TimeWindowTest : public ::testing::Test {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9900000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 9800000}};
  TimePoint tp4{milliseconds{std::numeric_limits<int64_t>::max() / 9500000}};
  TimePoint tp5{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};

  Duration dur1{seconds{100}};
  Duration dur2{seconds{1000}};
  Duration dur3{seconds{10000}};
};

TEST_F(TimeWindowTest, DefaultConstructor) {
  TimeWindow tw;

  EXPECT_EQ(tw.from(), TimePoint{});
  EXPECT_EQ(tw.to(), TimePoint{});
  EXPECT_EQ(tw.duration(), milliseconds{});
  EXPECT_FALSE(tw.contains(TimePoint{}));
  EXPECT_FALSE(tw.contains(0));
  EXPECT_TRUE(tw.contains(tw));
}

TEST_F(TimeWindowTest, InvalidTimeWindowFromTime) { EXPECT_THROW(TimeWindow(tp2, tp1), invalid_argument); }
TEST_F(TimeWindowTest, InvalidTimeWindowFromDuration) { EXPECT_THROW(TimeWindow(tp1, tp1 - tp2), invalid_argument); }

TEST_F(TimeWindowTest, DurationConstructor) {
  TimeWindow tw(tp1, tp2 - tp1);

  EXPECT_EQ(tw, TimeWindow(tp1, tp2));
}

TEST_F(TimeWindowTest, Duration) {
  TimeWindow tw(tp1, tp2);

  EXPECT_EQ(tw.duration(), tp2 - tp1);
}

TEST_F(TimeWindowTest, ContainsTimePoint) {
  TimeWindow tw1(tp1, tp2);

  EXPECT_TRUE(tw1.contains(tp1));
  EXPECT_TRUE(tw1.contains(tp1 + dur1));
  EXPECT_FALSE(tw1.contains(tp2));
  EXPECT_FALSE(tw1.contains(tp3));
}

TEST_F(TimeWindowTest, ContainsTimeWindow) {
  // [      ]
  //   [  ]
  TimeWindow tw1(tp1, tp4);
  TimeWindow tw2(tp2, tp3);

  EXPECT_TRUE(tw1.contains(tw1));
  EXPECT_TRUE(tw1.overlaps(tw1));

  EXPECT_TRUE(tw1.overlaps(tw2));
  EXPECT_TRUE(tw1.contains(tw2));

  EXPECT_TRUE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, OverlapNominal) {
  //       [      ]
  //   [     ]
  TimeWindow tw1(tp2, tp4);
  TimeWindow tw2(tp1, tp3);

  EXPECT_TRUE(tw1.overlaps(tw2));
  EXPECT_FALSE(tw1.contains(tw2));

  EXPECT_TRUE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, OverlapEqualTo) {
  //       [          ]
  //            [     ]
  TimeWindow tw1(tp1, tp3);
  TimeWindow tw2(tp2, tp3);

  EXPECT_TRUE(tw1.overlaps(tw2));
  EXPECT_TRUE(tw1.contains(tw2));

  EXPECT_TRUE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, OverlapEqualFrom) {
  //       [          ]
  //       [     ]
  TimeWindow tw1(tp1, tp3);
  TimeWindow tw2(tp1, tp2);

  EXPECT_TRUE(tw1.overlaps(tw2));
  EXPECT_TRUE(tw1.contains(tw2));

  EXPECT_TRUE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, NoOverlapNominal) {
  //       [    ]
  //               [     ]
  TimeWindow tw1(tp1, tp2);
  TimeWindow tw2(tp3, tp4);

  EXPECT_FALSE(tw1.overlaps(tw2));
  EXPECT_FALSE(tw1.contains(tw2));

  EXPECT_FALSE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, NoOverlapEqual) {
  //       [    ]
  //            [     ]
  TimeWindow tw1(tp1, tp3);
  TimeWindow tw2(tp3, tp4);

  EXPECT_FALSE(tw1.overlaps(tw2));
  EXPECT_FALSE(tw1.contains(tw2));

  EXPECT_FALSE(tw2.overlaps(tw1));
  EXPECT_FALSE(tw2.contains(tw1));
}

TEST_F(TimeWindowTest, OperatorPlus) {
  TimeWindow tw1(tp1, tp2);
  const TimeWindow expected(tp1 + dur1, tp2 + dur1);

  EXPECT_EQ(tw1 + dur1, expected);

  tw1 += dur1;

  EXPECT_EQ(tw1, expected);
}

TEST_F(TimeWindowTest, AggregateMinMax) {
  TimeWindow tw1(tp1, tp2);
  TimeWindow tw2(tp3, tp4);

  EXPECT_EQ(tw1.aggregateMinMax(tw2), TimeWindow(tp1, tp4));
}

TEST_F(TimeWindowTest, AggregateMinMaxWithNeutral) {
  TimeWindow tw1(tp1, tp2);
  TimeWindow tw2;

  EXPECT_EQ(tw1.aggregateMinMax(tw2), tw1);
  EXPECT_EQ(tw2.aggregateMinMax(tw1), tw1);
}

namespace {
constexpr std::string_view kExpectedTimeWindowStr = "[1999-03-25T04:46:43Z -> 1999-07-11T00:42:21Z)";
}

TEST_F(TimeWindowTest, Str) {
  TimeWindow tw(tp1, tp2);

  EXPECT_EQ(tw.str(), kExpectedTimeWindowStr);
}

TEST_F(TimeWindowTest, AppendTo) {
  TimeWindow tw(tp1, tp2);
  char buf[TimeWindow::strLen()];

  EXPECT_EQ(tw.appendTo(buf), buf + TimeWindow::strLen());
  EXPECT_EQ(std::string_view(buf, TimeWindow::strLen()), kExpectedTimeWindowStr);
}

TEST_F(TimeWindowTest, FromString) {
  TimeWindow tw(kExpectedTimeWindowStr);

  EXPECT_EQ(tw.str(), kExpectedTimeWindowStr);

  // For some reason, the timepoints are not exactly equal, they differ by a few hundreds of milliseconds
  EXPECT_LT(tw.from() > tp1 ? tw.from() - tp1 : tp1 - tw.from(), std::chrono::seconds{1});
  EXPECT_LT(tw.to() > tp2 ? tw.to() - tp2 : tp2 - tw.to(), std::chrono::seconds{1});
}

}  // namespace cct
