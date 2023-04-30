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

  EXPECT_TRUE(constraints.validateId("id0"));
  EXPECT_TRUE(constraints.validateId("id1"));
}

TEST(DepositsConstraintsTest, SingleIdWithCurrency) {
  WithdrawsOrDepositsConstraints constraints("XRP", "id0");

  EXPECT_TRUE(constraints.isCurDefined());
  EXPECT_FALSE(constraints.isAtMostCurDependent());

  EXPECT_TRUE(constraints.isIdDependent());
  EXPECT_FALSE(constraints.isIdOnlyDependent());

  EXPECT_FALSE(constraints.validateCur("KRW"));
  EXPECT_TRUE(constraints.validateCur("XRP"));
  EXPECT_FALSE(constraints.validateCur("EUR"));

  EXPECT_TRUE(constraints.validateId("id0"));
  EXPECT_FALSE(constraints.validateId("id1"));
}

}  // namespace cct