#include "proto-public-trade.hpp"

#include <gtest/gtest.h>

#include "monetaryamount.hpp"
#include "publictrade.hpp"

namespace cct {

class ProtoPublicTradeTest : public ::testing::Test {
 protected:
  TimePoint tp{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  Market market{"ETH", "USDT"};
  PublicTrade pt{TradeSide::kBuy, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp};
  TradeDataToPublicTradeConverter publicTradeConverter{market};
};

TEST_F(ProtoPublicTradeTest, SerializeThenDeserializeShouldGiveSameObject) {
  const auto protoObj = ConvertPublicTradeToTradeData(pt);

  const auto objBack = publicTradeConverter(protoObj);

  EXPECT_EQ(pt, objBack);
}
}  // namespace cct