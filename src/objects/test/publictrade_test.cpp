#include "publictrade.hpp"

#include <gtest/gtest.h>

namespace cct {
class PublicTradeTest : public ::testing::Test {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};

  Market market{"ETH", "USDT"};
  MonetaryAmount amount1{"3.7", market.base()};
  MonetaryAmount amount2{"0.13", market.base()};
  MonetaryAmount amount3{"0.55", market.base()};

  MonetaryAmount price1{"1500.5", market.quote()};
  MonetaryAmount price2{"1501", market.quote()};

  PublicTrade pt1{TradeSide::kBuy, amount1, price1, tp1};
  PublicTrade pt2{TradeSide::kSell, amount2, price2, tp2};
  PublicTrade pt3{TradeSide::kSell, amount3, price2, tp1};
};

TEST_F(PublicTradeTest, Validity) {
  EXPECT_TRUE(pt1.isValid());
  EXPECT_TRUE(pt2.isValid());
  EXPECT_TRUE(pt3.isValid());

  EXPECT_FALSE(PublicTrade(static_cast<TradeSide>(-1), amount1, price1, tp1).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, amount1, amount2, tp1).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, amount1, price1, TimePoint{}).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, MonetaryAmount{}, price1, tp1).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, amount1, MonetaryAmount{0, market.quote()}, tp1).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, -amount1, price1, tp1).isValid());
  EXPECT_FALSE(PublicTrade(TradeSide::kBuy, amount1, -price1, tp1).isValid());
}

TEST_F(PublicTradeTest, Members) {
  EXPECT_EQ(pt1.side(), TradeSide::kBuy);
  EXPECT_EQ(pt1.market(), market);
  EXPECT_EQ(pt1.amount(), amount1);
  EXPECT_EQ(pt1.price(), price1);
  EXPECT_EQ(pt1.time(), tp1);

  EXPECT_TRUE(pt1.isValid());
  EXPECT_EQ(pt1.timeStr(), "1999-03-25T04:46:43Z");
}

TEST_F(PublicTradeTest, Comparison) {
  EXPECT_NE(pt1, pt2);
  EXPECT_NE(pt1, pt3);

  EXPECT_LT(pt1, pt2);
  EXPECT_GT(pt1, pt3);
}
}  // namespace cct