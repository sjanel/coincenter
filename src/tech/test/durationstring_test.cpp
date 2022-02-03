#include "durationstring.hpp"

#include <gtest/gtest.h>

#include <chrono>

#include "cct_invalid_argument_exception.hpp"

namespace cct {

TEST(ParseDuration, EmptyDurationNotAllowed) { EXPECT_THROW(ParseDuration(""), invalid_argument); }

TEST(ParseDuration, DurationDays) { EXPECT_EQ(ParseDuration("37d"), std::chrono::days(37)); }

TEST(ParseDuration, DurationHours) { EXPECT_EQ(ParseDuration("12h"), std::chrono::hours(12)); }

TEST(ParseDuration, DurationMinutesSpaces) {
  EXPECT_EQ(ParseDuration("1 h 45      min "), std::chrono::hours(1) + std::chrono::minutes(45));
}

TEST(ParseDuration, DurationSeconds) { EXPECT_EQ(ParseDuration("3s"), std::chrono::seconds(3)); }

TEST(ParseDuration, DurationMilliseconds) { EXPECT_EQ(ParseDuration("1500 ms"), std::chrono::milliseconds(1500)); }

TEST(ParseDuration, DurationMicroseconds) {
  EXPECT_EQ(ParseDuration("567889358us"), std::chrono::microseconds(567889358));
}

TEST(ParseDuration, DurationLongTime) {
  EXPECT_EQ(ParseDuration("3y9mon2w5min"),
            std::chrono::years(3) + std::chrono::months(9) + std::chrono::weeks(2) + std::chrono::minutes(5));
}

TEST(ParseDuration, DurationThrowInvalidTimeUnit1) { EXPECT_THROW(ParseDuration("13z"), invalid_argument); }

TEST(ParseDuration, DurationThrowInvalidTimeUnit2) { EXPECT_THROW(ParseDuration("42"), invalid_argument); }

TEST(ParseDuration, DurationThrowOnlyIntegral) { EXPECT_THROW(ParseDuration("2.5min"), invalid_argument); }

TEST(DurationString, DurationToStringYears) { EXPECT_EQ(DurationToString(std::chrono::years(23)), "23y"); }
TEST(DurationString, DurationToStringMonths) { EXPECT_EQ(DurationToString(std::chrono::months(4)), "4mon"); }
TEST(DurationString, DurationToStringDays) { EXPECT_EQ(DurationToString(std::chrono::days(7)), "1w"); }
TEST(DurationString, DurationToStringDaysAndHours) {
  EXPECT_EQ(DurationToString(std::chrono::days(3) + std::chrono::hours(12)), "3d12h");
}
TEST(DurationString, DurationToStringWeeksDaysMinutes) {
  EXPECT_EQ(DurationToString(std::chrono::weeks(2) + std::chrono::days(6) + std::chrono::minutes(57)), "2w6d57min");
}
TEST(DurationString, DurationToStringYearsHoursSecondsMilliseconds) {
  EXPECT_EQ(DurationToString(std::chrono::years(50) + std::chrono::hours(2) + std::chrono::seconds(13) +
                             std::chrono::milliseconds(556)),
            "50y2h13s556ms");
}
TEST(DurationString, DurationToStringMicroseconds) {
  EXPECT_EQ(DurationToString(std::chrono::microseconds(31736913078454L)), "1y2d1h59min21s78ms454us");
}

}  // namespace cct