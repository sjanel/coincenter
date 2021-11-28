#include "timestring.hpp"

#include <gtest/gtest.h>

#include <regex>
#include <thread>

#include "stringhelpers.hpp"

namespace cct {

TEST(TimeStringTest, TimeSinceEpoch) {
  Nonce n1 = Nonce_TimeSinceEpoch();
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpoch();
  EXPECT_LT(n1, n2);
  EXPECT_LT(FromString<uint64_t>(n1), FromString<uint64_t>(n2));
}

TEST(TimeStringTest, LiteralDate) {
  Nonce n1 = Nonce_LiteralDate("%Y-%m-%d %H:%M:%S");
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  Nonce n2 = Nonce_LiteralDate("%Y-%m-%d %H:%M:%S");
  EXPECT_LT(n1, n2);

  const std::regex dateRegex("[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}");
  EXPECT_TRUE(std::regex_match(n1.begin(), n1.end(), dateRegex));
  EXPECT_TRUE(std::regex_match(n2.begin(), n2.end(), dateRegex));
}
}  // namespace cct