#include "closed-order.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class ClosedOrderTest : public ::testing::Test {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9900000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 9800000}};

  ClosedOrder closedOrder1{"1", MonetaryAmount(15, "BTC", 1), MonetaryAmount(35000, "USDT"), tp1, tp1, TradeSide::buy};
  ClosedOrder closedOrder2{"2", MonetaryAmount(25, "BTC", 1), MonetaryAmount(45000, "USDT"), tp2, tp3, TradeSide::buy};
};

TEST_F(ClosedOrderTest, SelfMerge) {
  ClosedOrder mergedClosedOrder = closedOrder1.mergeWith(closedOrder1);

  EXPECT_EQ(mergedClosedOrder.id(), closedOrder1.id());
  EXPECT_EQ(mergedClosedOrder.placedTime(), closedOrder1.placedTime());
  EXPECT_EQ(mergedClosedOrder.matchedVolume(), closedOrder1.matchedVolume() * 2);
  EXPECT_EQ(mergedClosedOrder.price(), closedOrder1.price());
  EXPECT_EQ(mergedClosedOrder.market(), closedOrder1.market());
  EXPECT_EQ(mergedClosedOrder.matchedTime(), closedOrder1.matchedTime());
}

TEST_F(ClosedOrderTest, Merge) {
  ClosedOrder mergedClosedOrder = closedOrder1.mergeWith(closedOrder2);

  EXPECT_EQ(mergedClosedOrder.id(), closedOrder1.id());
  EXPECT_EQ(mergedClosedOrder.placedTime(), closedOrder1.placedTime());
  EXPECT_EQ(mergedClosedOrder.matchedVolume(), closedOrder1.matchedVolume() + closedOrder2.matchedVolume());
  EXPECT_EQ(mergedClosedOrder.price(), MonetaryAmount(41250, closedOrder1.price().currencyCode()));
  EXPECT_EQ(mergedClosedOrder.market(), closedOrder1.market());
  EXPECT_EQ(mergedClosedOrder.matchedTime(), TimePoint{milliseconds{934101708833}});
}
}  // namespace cct