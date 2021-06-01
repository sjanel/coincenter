#include "cct_nonce.hpp"

#include <gtest/gtest.h>

#include <charconv>
#include <regex>
#include <thread>

namespace cct {

TEST(NonceTest, TimeSinceEpoch) {
  Nonce n1 = Nonce_TimeSinceEpoch();
  std::this_thread::sleep_for(std::chrono::milliseconds(2));
  Nonce n2 = Nonce_TimeSinceEpoch();
  EXPECT_LT(n1, n2);

  uint64_t iN1, iN2;
  std::from_chars(n1.data(), n1.data() + n1.size(), iN1);
  std::from_chars(n2.data(), n2.data() + n2.size(), iN2);
  EXPECT_LT(iN1, iN2);
}

TEST(NonceTest, LiteralDate) {
  Nonce n1 = Nonce_LiteralDate();
  std::this_thread::sleep_for(std::chrono::milliseconds(1100));
  Nonce n2 = Nonce_LiteralDate();
  EXPECT_LT(n1, n2);

  const std::regex dateRegex("[0-9]{4}-[0-9]{2}-[0-9]{2}T[0-9]{2}:[0-9]{2}:[0-9]{2}");
  EXPECT_TRUE(std::regex_match(n1, dateRegex));
  EXPECT_TRUE(std::regex_match(n2, dateRegex));
}
}  // namespace cct