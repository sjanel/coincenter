#include "depositsconstraints.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(DepositsConstraintsBitmapTest, Empty) {
  DepositsConstraintsBitmap bmp;
  EXPECT_TRUE(bmp.empty());
  EXPECT_FALSE(bmp.isCurDependent());
  EXPECT_TRUE(bmp.isAtMostCurOnlyDependent());
}

TEST(DepositConstraintsBitmapTest, MarketOnlyDependent) {
  DepositsConstraintsBitmap bmp;
  bmp.set(DepositsConstraintsBitmap::ConstraintType::kCur);
  EXPECT_TRUE(bmp.isAtMostCurOnlyDependent());
  EXPECT_TRUE(bmp.isCurDependent());
  EXPECT_TRUE(bmp.isCurOnlyDependent());
  EXPECT_FALSE(bmp.isDepositIdOnlyDependent());
}

TEST(DepositsConstraintsTest, Empty) {
  DepositsConstraints depositsConstraints;
  EXPECT_TRUE(depositsConstraints.noConstraints());
  EXPECT_FALSE(depositsConstraints.isReceivedTimeAfterDefined());
  EXPECT_TRUE(depositsConstraints.validateCur("KRW"));
}

TEST(DepositsConstraintsTest, Currency) {
  DepositsConstraints depositsConstraints("BTC");
  EXPECT_TRUE(depositsConstraints.isCurDefined());
  EXPECT_TRUE(depositsConstraints.isAtMostCurDependent());
  EXPECT_FALSE(depositsConstraints.isOrderIdDependent());
  EXPECT_FALSE(depositsConstraints.validateCur("KRW"));
  EXPECT_TRUE(depositsConstraints.validateCur("BTC"));
  EXPECT_FALSE(depositsConstraints.validateCur("EUR"));
}

}  // namespace cct