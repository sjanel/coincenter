#include "unitsparser.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
TEST(UnitsParser, ParseNumberOfBytes1kMultipliers) {
  EXPECT_EQ(ParseNumberOfBytes("748"), 748L);
  EXPECT_EQ(ParseNumberOfBytes("788999k"), 788999000L);
  EXPECT_EQ(ParseNumberOfBytes("34M"), 34000000L);
  EXPECT_EQ(ParseNumberOfBytes("1G"), 1000000000L);
  EXPECT_EQ(ParseNumberOfBytes("5T"), 5000000000000L);
}

TEST(UnitsParser, ParseNumberOfBytes1024Multipliers) {
  EXPECT_EQ(ParseNumberOfBytes("12"), 12L);
  EXPECT_EQ(ParseNumberOfBytes("3Ki"), 3072L);
  EXPECT_EQ(ParseNumberOfBytes("5Mi"), 5242880L);
  EXPECT_EQ(ParseNumberOfBytes("57Gi"), 61203283968L);
  EXPECT_EQ(ParseNumberOfBytes("2Ti"), 2199023255552L);
}

TEST(UnitsParser, ParseNumberOfBytesInvalidInput) {
  EXPECT_THROW(ParseNumberOfBytes("12.5M"), exception);
  EXPECT_THROW(ParseNumberOfBytes("400m"), exception);
  EXPECT_THROW(ParseNumberOfBytes("-30"), exception);
}
}  // namespace cct