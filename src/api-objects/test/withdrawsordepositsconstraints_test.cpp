#include "withdrawsordepositsconstraints.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(DepositsConstraintsTest, Empty) {
  WithdrawsOrDepositsConstraints constraints;
  EXPECT_TRUE(constraints.noConstraints());
  EXPECT_FALSE(constraints.isTimeAfterDefined());
  EXPECT_TRUE(constraints.validateCur("KRW"));
}

TEST(DepositsConstraintsTest, Currency) {
  WithdrawsOrDepositsConstraints constraints("BTC");
  EXPECT_TRUE(constraints.isCurDefined());
  EXPECT_TRUE(constraints.isAtMostCurDependent());
  EXPECT_FALSE(constraints.isIdDependent());
  EXPECT_FALSE(constraints.validateCur("KRW"));
  EXPECT_TRUE(constraints.validateCur("BTC"));
  EXPECT_FALSE(constraints.validateCur("EUR"));
}

}  // namespace cct