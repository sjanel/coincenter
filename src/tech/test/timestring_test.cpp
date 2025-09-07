#include "timestring.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <regex>
#include <thread>

#include "cct_invalid_argument_exception.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"

namespace cct {

using namespace std::chrono;

TEST(TimeStringTest, TimeSinceEpoch) {
  Nonce n1 = Nonce_TimeSinceEpochInMs();
  std::this_thread::sleep_for(milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpochInMs();
  EXPECT_LT(n1, n2);
  EXPECT_LT(StringToIntegral<uint64_t>(n1), StringToIntegral<uint64_t>(n2));
}

TEST(TimeStringTest, TimeSinceEpochDelay) {
  Nonce n1 = Nonce_TimeSinceEpochInMs(seconds(1));
  std::this_thread::sleep_for(milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpochInMs();
  EXPECT_GT(n1, n2);
  EXPECT_GT(StringToIntegral<uint64_t>(n1), StringToIntegral<uint64_t>(n2));
}

TEST(TimeStringTest, LiteralDate) {
  Nonce n1 = Nonce_LiteralDate(kTimeYearToSecondSpaceSeparatedFormat);
  std::this_thread::sleep_for(milliseconds(1020));
  Nonce n2 = Nonce_LiteralDate(kTimeYearToSecondSpaceSeparatedFormat);
  EXPECT_LT(n1, n2);

  const std::regex dateRegex("[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}");
  EXPECT_TRUE(std::regex_match(n1.begin(), n1.end(), dateRegex));
  EXPECT_TRUE(std::regex_match(n2.begin(), n2.end(), dateRegex));
}

TEST(TimeStringTest, TimeToString) {
  TimePoint tp;
  tp += std::chrono::years(15);
  tp += std::chrono::months(9);
  tp += std::chrono::days(25);

  EXPECT_EQ(TimeToString(tp, "%Y"), "1985");
  EXPECT_EQ(TimeToString(tp, "%Y-%m"), "1985-10");
  EXPECT_EQ(TimeToString(tp, "%Y-%m-%d"), "1985-10-26");
  EXPECT_EQ(TimeToString(tp, "%Y-%m-%d %H"), "1985-10-26 13");
  EXPECT_EQ(TimeToString(tp, "%Y-%m-%d %H:%M"), "1985-10-26 13:39");
  EXPECT_EQ(TimeToString(tp, "%Y-%m-%d %H:%M:%S"), "1985-10-26 13:39:54");
  EXPECT_EQ(TimeToString(tp, "%Y-%m-%d W%U %H:%M:%S"), "1985-10-26 W42 13:39:54");

  EXPECT_EQ(TimeToString(tp, "%D - %T"), "10/26/85 - 13:39:54");
  EXPECT_EQ(TimeToString(tp, "%D custom string %T"), "10/26/85 custom string 13:39:54");
}

TEST(TimeStringTest, FromToString) {
  std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  EXPECT_EQ(TimeToString(now), TimeToString(StringToTime(TimeToString(now).c_str())));
}

TEST(TimeStringIso8601UTCTest, BasicIso8601Format) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 8 / 14} + std::chrono::hours{12} +
                 std::chrono::minutes{34} + std::chrono::seconds{56} + std::chrono::milliseconds{789};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2025-08-14T12:34:56.789Z");
}

TEST(TimeStringIso8601UTCTest, Midnight) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2022} / 1 / 1};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2022-01-01T00:00:00.000Z");
}

TEST(TimeStringIso8601UTCTest, EndOfYear) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2023} / 12 / 31} + std::chrono::hours{23} +
                 std::chrono::minutes{59} + std::chrono::seconds{59} + std::chrono::milliseconds{999};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2023-12-31T23:59:59.999Z");
}

TEST(TimeStringIso8601UTCTest, LeapYearFeb29) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2024} / 2 / 29} + std::chrono::hours{6} +
                 std::chrono::minutes{30} + std::chrono::seconds{15} + std::chrono::milliseconds{123};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2024-02-29T06:30:15.123Z");
}

TEST(TimeStringIso8601UTCTest, SingleDigitMonthDay) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 3 / 7} + std::chrono::hours{1} +
                 std::chrono::minutes{2} + std::chrono::seconds{3} + std::chrono::milliseconds{4};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2025-03-07T01:02:03.004Z");
}

TEST(TimeStringIso8601UTCTest, ZeroMilliseconds) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 8 / 14} + std::chrono::hours{12} +
                 std::chrono::minutes{34} + std::chrono::seconds{56};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2025-08-14T12:34:56.000Z");
}

TEST(TimeStringIso8601UTCTest, MaximumMilliseconds) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 8 / 14} + std::chrono::hours{23} +
                 std::chrono::minutes{59} + std::chrono::seconds{59} + std::chrono::milliseconds{999};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "2025-08-14T23:59:59.999Z");
}

TEST(TimeStringIso8601UTCTest, MinimumDate) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{1970} / 1 / 1};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  EXPECT_EQ(std::string(buf, end - buf), "1970-01-01T00:00:00.000Z");
}

TEST(TimeStringIso8601UTCTest, NegativeMilliseconds) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 8 / 14} + std::chrono::hours{12} +
                 std::chrono::minutes{34} + std::chrono::seconds{56} - std::chrono::milliseconds{1};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  // Should roll back to previous second
  EXPECT_EQ(std::string(buf, end - buf), "2025-08-14T12:34:55.999Z");
}

TEST(TimeStringIso8601UTCTest, RoundTripConversion) {
  char buf[32];
  TimePoint tp = std::chrono::sys_days{std::chrono::year{2025} / 8 / 14} + std::chrono::hours{12} +
                 std::chrono::minutes{34} + std::chrono::seconds{56} + std::chrono::milliseconds{789};
  char* end = TimeToStringIso8601UTCWithMillis(tp, buf);
  std::string iso(buf, end - buf);
  TimePoint tp2 = StringToTimeISO8601UTC(iso.c_str(), iso.c_str() + iso.size());
  char buf2[32];
  char* end2 = TimeToStringIso8601UTCWithMillis(tp2, buf2);
  EXPECT_EQ(std::string(buf2, end2 - buf2), iso);
}

class StringToTimeISO8601UTCTest : public ::testing::Test {};

// ------------------------ Valid cases ------------------------
TEST_F(StringToTimeISO8601UTCTest, ParsesBasicISO8601UTC) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56Z");
  auto sys_days = floor<days>(tp);
  auto ymd = year_month_day(sys_days);
  EXPECT_EQ(int(ymd.year()), 2025);
  EXPECT_EQ(unsigned(ymd.month()), 8);
  EXPECT_EQ(unsigned(ymd.day()), 14);

  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<hours>(timeOfDay).count(), 12);
  EXPECT_EQ(duration_cast<minutes>(timeOfDay).count() % 60, 34);
  EXPECT_EQ(duration_cast<seconds>(timeOfDay).count() % 60, 56);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesISO8601UTCWithoutZ) {
  auto tp = StringToTimeISO8601UTC("2025-08-14 12:34:56");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<hours>(timeOfDay).count(), 12);
  EXPECT_EQ(duration_cast<minutes>(timeOfDay).count() % 60, 34);
  EXPECT_EQ(duration_cast<seconds>(timeOfDay).count() % 60, 56);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesWithMilliseconds) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.123Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<milliseconds>(timeOfDay).count() % 1000, 123);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesWithMicroseconds) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.123456Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<microseconds>(timeOfDay).count() % 1000000, 123456);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesWithNanoseconds) {
  auto tp = StringToTimeISO8601UTC("2025-08-08T18:00:00.000864693Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});

  auto rem = duration_cast<nanoseconds>(timeOfDay).count() % 1'000'000'000;
  using SysDur = std::chrono::system_clock::duration;
  // Compute system_clock tick size in nanoseconds: (1e9 * num / den)
  constexpr int64_t tick_ns = static_cast<int64_t>((1'000'000'000LL * SysDur::period::num) / SysDur::period::den);
  EXPECT_LE(std::llabs(rem - 864693), tick_ns - 1);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesWithCustomSubSecondPrecision) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.1234567Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<nanoseconds>(timeOfDay).count() % 1000000000, 123456700);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesSpaceInsteadOfT) {
  auto tp = StringToTimeISO8601UTC("2025-08-14 12:34:56Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<hours>(timeOfDay).count(), 12);
  EXPECT_EQ(duration_cast<minutes>(timeOfDay).count() % 60, 34);
  EXPECT_EQ(duration_cast<seconds>(timeOfDay).count() % 60, 56);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesWithoutSecondsFraction) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T00:00:00Z");
  auto sys_days = floor<days>(tp);
  auto timeOfDay = tp - sys_days;

  EXPECT_GE(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<seconds>(timeOfDay).count(), 0);
}

// ------------------------ Edge cases ------------------------
TEST_F(StringToTimeISO8601UTCTest, ParsesStartOfMonth) {
  auto tp = StringToTimeISO8601UTC("2025-08-01T00:00:00Z");
  auto ymd = year_month_day(floor<days>(tp));
  EXPECT_EQ(unsigned(ymd.day()), 1);
}

TEST_F(StringToTimeISO8601UTCTest, ParsesEndOfYear) {
  auto tp = StringToTimeISO8601UTC("2025-12-31T23:59:59Z");
  auto ymd = year_month_day(floor<days>(tp));
  EXPECT_EQ(unsigned(ymd.month()), 12);
  EXPECT_EQ(unsigned(ymd.day()), 31);
}

// ------------------------ Invalid cases ------------------------
TEST_F(StringToTimeISO8601UTCTest, ThrowsOnTooShortString) {
  EXPECT_THROW(StringToTimeISO8601UTC("2025-08"), invalid_argument);
  EXPECT_THROW(StringToTimeISO8601UTC("2025-08-14"), invalid_argument);
  EXPECT_THROW(StringToTimeISO8601UTC("2025-08-14 11"), invalid_argument);
  EXPECT_THROW(StringToTimeISO8601UTC("2025-08-14 11:22"), invalid_argument);
}

TEST_F(StringToTimeISO8601UTCTest, ThrowsOnEmptyString) { EXPECT_THROW(StringToTimeISO8601UTC(""), invalid_argument); }

// ------------------------ Sub-second edge cases ------------------------
TEST_F(StringToTimeISO8601UTCTest, Handles1DigitSubsecond) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.1Z");
  auto timeOfDay = tp - floor<days>(tp);
  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<nanoseconds>(timeOfDay).count() % 1000000000, 100'000'000);
}

TEST_F(StringToTimeISO8601UTCTest, Handles2DigitSubsecond) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.12Z");
  auto timeOfDay = tp - floor<days>(tp);
  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<nanoseconds>(timeOfDay).count() % 1000000000, 120'000'000);
}

TEST_F(StringToTimeISO8601UTCTest, Handles7DigitSubsecond) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.12345670Z");
  auto timeOfDay = tp - floor<days>(tp);
  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});
  EXPECT_EQ(duration_cast<nanoseconds>(timeOfDay).count() % 1000000000, 123456700);
}

TEST_F(StringToTimeISO8601UTCTest, Handles10DigitSubsecond) {
  auto tp = StringToTimeISO8601UTC("2025-08-14T12:34:56.3508191888");
  auto timeOfDay = tp - floor<days>(tp);
  EXPECT_GT(timeOfDay, std::chrono::nanoseconds{0});
  EXPECT_LT(timeOfDay, std::chrono::days{1});

  timeOfDay -= std::chrono::hours{12} + std::chrono::minutes{34} + std::chrono::seconds{56};
  // We provided 10 fractional digits; parser keeps up to 9 (nanosecond precision) and truncates the rest.
  // On platforms where system_clock has coarser resolution (e.g. 100ns on Windows), the stored value is quantized.
  auto ns = duration_cast<nanoseconds>(timeOfDay).count();
  constexpr int64_t expected_ns = 350819188;  // after truncating the 10th digit
  using sys_period = std::chrono::system_clock::period;
  // size (in ns) of one system_clock tick
  const int64_t tick_ns = (1'000'000'000LL * sys_period::num) / sys_period::den;
  const int64_t quantized_expected = (expected_ns / tick_ns) * tick_ns;
  EXPECT_EQ(ns, quantized_expected) << "(system_clock tick=" << tick_ns << "ns, expected raw=" << expected_ns << ")";
}

}  // namespace cct
