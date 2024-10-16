#include "durationstring.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <span>
#include <string_view>

#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "timedef.hpp"

namespace cct {

TEST(DurationLen, Basic) { EXPECT_EQ(DurationLen("99min"), 5); }

TEST(DurationLen, BasicComplex) { EXPECT_EQ(DurationLen("34d45min"), 8); }

TEST(DurationLen, BasicWithComma) { EXPECT_EQ(DurationLen("23s,bithumb"), 3); }

TEST(DurationLen, ComplexWithSpaces) { EXPECT_EQ(DurationLen(" 1 d 52 h,kraken"), 9); }

TEST(DurationLen, NegativeValue) { EXPECT_EQ(DurationLen("-3sec"), 0); }

TEST(DurationLen, InvalidTimeUnit) { EXPECT_EQ(DurationLen("63po"), 0); }

TEST(DurationLen, DoesNotStartWithNumber) { EXPECT_EQ(DurationLen("us"), 0); }

TEST(ParseDuration, EmptyDurationNotAllowed) { EXPECT_THROW(ParseDuration(""), invalid_argument); }

TEST(ParseDuration, DurationDays) { EXPECT_EQ(ParseDuration("37d"), std::chrono::days(37)); }

TEST(ParseDuration, DurationHours) { EXPECT_EQ(ParseDuration("12h"), std::chrono::hours(12)); }

TEST(ParseDuration, DurationMinutesSpaces) {
  EXPECT_EQ(ParseDuration("1 h 45      min "), std::chrono::hours(1) + std::chrono::minutes(45));
}

TEST(ParseDuration, DurationSeconds) { EXPECT_EQ(ParseDuration("3s"), seconds(3)); }

TEST(ParseDuration, DurationMilliseconds) { EXPECT_EQ(ParseDuration("1500 ms"), milliseconds(1500)); }

TEST(ParseDuration, DurationMicroseconds) { EXPECT_EQ(ParseDuration("567889358us"), microseconds(567889358)); }

TEST(ParseDuration, DurationLongTime) {
  EXPECT_EQ(ParseDuration("3y9mon2w5min"),
            std::chrono::years(3) + std::chrono::months(9) + std::chrono::weeks(2) + std::chrono::minutes(5));
}

TEST(ParseDuration, DurationThrowInvalidTimeUnit1) { EXPECT_THROW(ParseDuration("13z"), invalid_argument); }

TEST(ParseDuration, DurationThrowInvalidTimeUnit2) { EXPECT_THROW(ParseDuration("42"), invalid_argument); }

TEST(ParseDuration, DurationThrowOnlyIntegral) { EXPECT_THROW(ParseDuration("2.5min"), invalid_argument); }

TEST(DurationString, DurationToStringUndefined) { EXPECT_EQ(DurationToString(kUndefinedDuration), "<undef>"); }

TEST(DurationString, DurationToStringYears) { EXPECT_EQ(DurationToString(std::chrono::years(23)), "23y"); }
TEST(DurationString, DurationToStringMonths) { EXPECT_EQ(DurationToString(std::chrono::months(4)), "4mon"); }
TEST(DurationString, DurationToStringDays) { EXPECT_EQ(DurationToString(std::chrono::days(7)), "1w"); }
TEST(DurationString, DurationToStringDaysAndHours) {
  EXPECT_EQ(DurationToString(std::chrono::days(3) + std::chrono::hours(12)), "3d12h");
}
TEST(DurationString, DurationToStringWeeksDaysMinutes) {
  EXPECT_EQ(DurationToString(std::chrono::weeks(2) + std::chrono::days(6) + std::chrono::minutes(57), 3), "2w6d57min");
}
TEST(DurationString, DurationToStringYearsHoursSecondsMilliseconds) {
  EXPECT_EQ(DurationToString(std::chrono::years(50) + std::chrono::hours(2) + seconds(13) + milliseconds(556), 10),
            "50y2h13s556ms");
}
TEST(DurationString, DurationToStringMicroseconds) {
  EXPECT_EQ(DurationToString(microseconds(31736913078454L), 20), "1y2d1h59min21s78ms454us");
}
TEST(DurationString, DurationToStringMicrosecondsTruncated) {
  EXPECT_EQ(DurationToString(microseconds(31736913078454L), 2), "1y2d");
}

TEST(DurationString, DurationToBufferUndefEnoughBuffer) {
  char buffer[10];
  std::span<char> bufSpan(buffer);

  auto actualBuf = DurationToBuffer(kUndefinedDuration, bufSpan);

  EXPECT_EQ(std::string_view(buffer, actualBuf.size()), "<undef>");
}

TEST(DurationString, DurationToBufferUndefNotEnoughBuffer) {
  char buffer[6];
  std::span<char> bufSpan(buffer);

  EXPECT_THROW(DurationToBuffer(kUndefinedDuration, bufSpan), exception);
}

TEST(DurationString, DurationToBufferNotEnoughBuffer) {
  char buffer[7];
  std::span<char> bufSpan(buffer);

  EXPECT_THROW(DurationToBuffer(std::chrono::weeks(2) + std::chrono::days(6) + std::chrono::minutes(57), bufSpan, 3),
               exception);
}

TEST(DurationString, DurationToBufferNominal) {
  char buffer[100];
  std::span<char> bufSpan(buffer);

  auto actualBuf =
      DurationToBuffer(std::chrono::weeks(2) + std::chrono::days(6) + std::chrono::minutes(57), bufSpan, 3);

  EXPECT_EQ(std::string_view(buffer, actualBuf.size()), "2w6d57min");
}

}  // namespace cct
