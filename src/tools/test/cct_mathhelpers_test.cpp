#include "cct_mathhelpers.hpp"

#include <gtest/gtest.h>

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

TEST(MathHelpers, NDigits) {
  EXPECT_EQ(ndigits(0), 1);
  EXPECT_EQ(ndigits(-3), 1);
  EXPECT_EQ(ndigits(-78), 2);
  EXPECT_EQ(ndigits(170), 3);
  EXPECT_EQ(ndigits(-9245), 4);
  EXPECT_EQ(ndigits(100000), 6);
  EXPECT_EQ(ndigits(35710), 5);
  EXPECT_EQ(ndigits(1035710), 7);
  EXPECT_EQ(ndigits(-5905614858), 10);
  EXPECT_EQ(ndigits(-908561485), 9);
  EXPECT_EQ(ndigits(18561485), 8);
  EXPECT_EQ(ndigits(-1861485), 7);
  EXPECT_EQ(ndigits(-186148), 6);
  EXPECT_EQ(ndigits(36816), 5);
  EXPECT_EQ(ndigits(-3686), 4);
  EXPECT_EQ(ndigits(686), 3);
  EXPECT_EQ(ndigits(-10), 2);
  EXPECT_EQ(ndigits(1), 1);
  static_assert(ndigits(std::numeric_limits<int32_t>::max()) == 10);
  static_assert(ndigits(std::numeric_limits<int32_t>::min()) == 10);

  EXPECT_EQ(ndigits(std::numeric_limits<int64_t>::max()), 19);
  EXPECT_EQ(ndigits(7299385028562659L), 16);
  EXPECT_EQ(ndigits(299385028562659L), 15);
  EXPECT_EQ(ndigits(29938502856265L), 14);
  EXPECT_EQ(ndigits(2938502856265L), 13);
  EXPECT_EQ(ndigits(-590385614858L), 12);
  EXPECT_EQ(ndigits(-59085614858L), 11);
  EXPECT_EQ(ndigits(-5905614858L), 10);
  EXPECT_EQ(ndigits(-908561485), 9);
  EXPECT_EQ(ndigits(18561485), 8);
  EXPECT_EQ(ndigits(1861485), 7);
  EXPECT_EQ(ndigits(186148), 6);
  EXPECT_EQ(ndigits(36816), 5);
  EXPECT_EQ(ndigits(3686), 4);
  EXPECT_EQ(ndigits(686), 3);
  EXPECT_EQ(ndigits(10), 2);
  EXPECT_EQ(ndigits(0), 1);
  static_assert(ndigits(72993850285626590L) == 17);
}

}  // namespace cct
