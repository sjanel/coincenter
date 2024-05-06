#include "ipow.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace cct {

TEST(MathHelpers, Power) {
  EXPECT_EQ(ipow(3, 2), 9);
  EXPECT_EQ(ipow(4, 3), 64);
  EXPECT_EQ(ipow(-3, 3), -27);
  EXPECT_EQ(ipow(-2, 1), -2);
  EXPECT_EQ(ipow(17, 5), 1419857);
  EXPECT_EQ(ipow(10, 10), 10000000000);
  static_assert(ipow(5, 3) == 125);
  static_assert(ipow(-7, 0) == 1);
}

TEST(MathHelpers, Power10) {
  EXPECT_EQ(ipow10(0), 1);
  EXPECT_EQ(ipow10(1), 10);
  EXPECT_EQ(ipow10(2), 100);
  EXPECT_EQ(ipow10(10), 10000000000);
  static_assert(ipow10(3) == 1000);
}
}  // namespace cct