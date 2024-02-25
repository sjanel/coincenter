#include "ordersconstraints.hpp"

#include <gtest/gtest.h>

#include "market.hpp"

namespace cct {
TEST(OrderConstraintsBitmapTest, Empty) {
  OrderConstraintsBitmap bmp;
  EXPECT_TRUE(bmp.empty());
  EXPECT_FALSE(bmp.isMarketDependent());
  EXPECT_TRUE(bmp.isAtMostMarketOnlyDependent());
}

TEST(OrderConstraintsBitmapTest, MarketOnlyDependent) {
  OrderConstraintsBitmap bmp;
  bmp.set(OrderConstraintsBitmap::ConstraintType::kCur1);
  EXPECT_TRUE(bmp.isAtMostMarketOnlyDependent());
  EXPECT_FALSE(bmp.isMarketDependent());
  bmp.set(OrderConstraintsBitmap::ConstraintType::kCur2);
  EXPECT_TRUE(bmp.isMarketOnlyDependent());
  EXPECT_FALSE(bmp.isOrderIdOnlyDependent());
}

TEST(OrderConstraintsTest, Empty) {
  OrdersConstraints orderConstraints;
  EXPECT_TRUE(orderConstraints.noConstraints());
  EXPECT_FALSE(orderConstraints.isPlacedTimeAfterDefined());
}

TEST(OrderConstraintsTest, Market1) {
  OrdersConstraints orderConstraints("BTC");
  EXPECT_TRUE(orderConstraints.isCurDefined());
  EXPECT_FALSE(orderConstraints.isCur2Defined());
  EXPECT_TRUE(orderConstraints.isAtMostMarketDependent());
  EXPECT_FALSE(orderConstraints.isOrderIdDependent());
  EXPECT_TRUE(orderConstraints.validateCur("KRW", "BTC"));
  EXPECT_TRUE(orderConstraints.validateCur("BTC", "EUR"));
  EXPECT_FALSE(orderConstraints.validateCur("KRW", "ETH"));
}

TEST(OrderConstraintsTest, Market2) {
  OrdersConstraints orderConstraints("BTC", "EUR");
  EXPECT_TRUE(orderConstraints.isCurDefined());
  EXPECT_TRUE(orderConstraints.isCur2Defined());
  EXPECT_TRUE(orderConstraints.isAtMostMarketDependent());
  EXPECT_EQ(orderConstraints.market(), Market("BTC", "EUR"));
  EXPECT_TRUE(orderConstraints.orderIdSet().empty());
  EXPECT_FALSE(orderConstraints.validateCur("KRW", "BTC"));
  EXPECT_TRUE(orderConstraints.validateCur("EUR", "BTC"));
  EXPECT_TRUE(orderConstraints.validateCur("BTC", "EUR"));
  EXPECT_FALSE(orderConstraints.validateCur("BTC", "USD"));
}

}  // namespace cct