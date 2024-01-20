#include "tradeoptions.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(TradeOptionsTest, DefaultTradeTimeoutAction) {
  TradeOptions tradeOptions;

  EXPECT_FALSE(tradeOptions.placeMarketOrderAtTimeout());
  EXPECT_EQ(tradeOptions.timeoutActionStr(), "cancel");
}
}  // namespace cct