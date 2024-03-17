#include "timestring.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <regex>
#include <thread>

#include "stringhelpers.hpp"
#include "timedef.hpp"

namespace cct {

TEST(TimeStringTest, TimeSinceEpoch) {
  Nonce n1 = Nonce_TimeSinceEpochInMs();
  std::this_thread::sleep_for(milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpochInMs();
  EXPECT_LT(n1, n2);
  EXPECT_LT(FromString<uint64_t>(n1), FromString<uint64_t>(n2));
}

TEST(TimeStringTest, TimeSinceEpochDelay) {
  Nonce n1 = Nonce_TimeSinceEpochInMs(seconds(1));
  std::this_thread::sleep_for(milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpochInMs();
  EXPECT_GT(n1, n2);
  EXPECT_GT(FromString<uint64_t>(n1), FromString<uint64_t>(n2));
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

TEST(TimeStringTest, ToString) {
  TimePoint tp;
  tp += std::chrono::years(15);
  tp += std::chrono::months(9);
  tp += std::chrono::days(25);

  EXPECT_EQ(ToString(tp, "%Y"), "1985");
  EXPECT_EQ(ToString(tp, "%Y-%m"), "1985-10");
  EXPECT_EQ(ToString(tp, "%Y-%m-%d"), "1985-10-26");
  EXPECT_EQ(ToString(tp, "%Y-%m-%d %H"), "1985-10-26 13");
  EXPECT_EQ(ToString(tp, "%Y-%m-%d %H:%M"), "1985-10-26 13:39");
  EXPECT_EQ(ToString(tp, "%Y-%m-%d %H:%M:%S"), "1985-10-26 13:39:54");
  EXPECT_EQ(ToString(tp, "%Y-%m-%d W%U %H:%M:%S"), "1985-10-26 W42 13:39:54");

  EXPECT_EQ(ToString(tp, "%D - %T"), "10/26/85 - 13:39:54");
  EXPECT_EQ(ToString(tp, "%D custom string %T"), "10/26/85 custom string 13:39:54");
}

TEST(TimeStringTest, FromToString) {
  // TODO: below lines should be uncommented
  // std::chrono::system_clock::time_point now = std::chrono::system_clock::now();
  // EXPECT_STREQ(ToString(now).c_str(), ToString(FromString(ToString(now).c_str())).c_str());
}
}  // namespace cct
