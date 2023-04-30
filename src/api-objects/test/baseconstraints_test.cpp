#include "baseconstraints.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(DepositsConstraintsBitmapTest, Empty) {
  CurrencyIdTimeConstraintsBmp bmp;
  EXPECT_TRUE(bmp.empty());
  EXPECT_FALSE(bmp.isCurDependent());
  EXPECT_TRUE(bmp.isAtMostCurOnlyDependent());
}

TEST(DepositConstraintsBitmapTest, MarketOnlyDependent) {
  CurrencyIdTimeConstraintsBmp bmp;
  bmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kCur);
  EXPECT_TRUE(bmp.isAtMostCurOnlyDependent());
  EXPECT_TRUE(bmp.isCurDependent());
  EXPECT_TRUE(bmp.isCurOnlyDependent());
  EXPECT_FALSE(bmp.isDepositIdOnlyDependent());
}

}  // namespace cct