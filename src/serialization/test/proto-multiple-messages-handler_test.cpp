#include "proto-multiple-messages-handler.hpp"

#include <gtest/gtest.h>

#include <sstream>
#include <type_traits>

#include "proto-public-trade.hpp"
#include "publictrade.hpp"
#include "trade-data.pb.h"

namespace cct {
class ProtobufMessagesTest : public ::testing::Test {
 protected:
  ProtobufMessagesWriter<std::stringstream> writer;

  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};

  Market market{"ETH", "USDT"};
  TradeDataToPublicTradeConverter protoTradeDataConverter{market};

  PublicTrade pt1{TradeSide::kBuy, MonetaryAmount{"0.13", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp1};
  PublicTrade pt2{TradeSide::kSell, MonetaryAmount{"3.7", "ETH"}, MonetaryAmount{"1500.5", "USDT"}, tp2};
  PublicTrade pt3{TradeSide::kBuy, MonetaryAmount{"0.004", "ETH"}, MonetaryAmount{1501, "USDT"}, tp3};

  ::objects::TradeData td1{ConvertPublicTradeToTradeData(pt1)};
  ::objects::TradeData td2{ConvertPublicTradeToTradeData(pt2)};
  ::objects::TradeData td3{ConvertPublicTradeToTradeData(pt3)};
};

TEST_F(ProtobufMessagesTest, WriteReadSingle) {
  writer.open(std::stringstream{});
  writer.write(td1);

  std::stringstream ss = writer.flush();

  ProtobufMessagesReader reader{ss};

  int nbObjectsRead = 0;

  while (reader.hasNext()) {
    auto nextObj = reader.next<::objects::TradeData>();
    PublicTrade pt = protoTradeDataConverter(nextObj);

    EXPECT_EQ(pt, pt1);
    ++nbObjectsRead;
  }
  EXPECT_EQ(nbObjectsRead, 1);
}

TEST_F(ProtobufMessagesTest, WriteRead2Flushes) {
  writer.open(std::stringstream{});
  writer.write(td1);
  std::stringstream ss1 = writer.flush();

  writer.open(std::stringstream{});
  writer.write(td2);
  std::stringstream ss2 = writer.flush();

  ProtobufMessagesReader reader1{ss1};

  int nbObjectsRead = 0;

  while (reader1.hasNext()) {
    auto nextObj = reader1.next<::objects::TradeData>();
    PublicTrade pt = protoTradeDataConverter(nextObj);

    EXPECT_EQ(pt, pt1);
    ++nbObjectsRead;
  }
  EXPECT_EQ(nbObjectsRead, 1);

  ProtobufMessagesReader reader2{ss2};

  while (reader2.hasNext()) {
    auto nextObj = reader2.next<::objects::TradeData>();
    PublicTrade pt = protoTradeDataConverter(nextObj);

    EXPECT_EQ(pt, pt2);
    ++nbObjectsRead;
  }
  EXPECT_EQ(nbObjectsRead, 2);
}

TEST_F(ProtobufMessagesTest, WriteReadSeveral) {
  writer.open(std::stringstream{});
  writer.write(td1);
  writer.write(td2);
  writer.write(td3);

  std::stringstream ss = writer.flush();

  ProtobufMessagesReader reader{ss};

  int nbObjectsRead = 0;

  while (reader.hasNext()) {
    auto nextObj = reader.next<::objects::TradeData>();
    PublicTrade pt = protoTradeDataConverter(nextObj);

    switch (nbObjectsRead) {
      case 0:
        EXPECT_EQ(pt, pt1);
        break;
      case 1:
        EXPECT_EQ(pt, pt2);
        break;
      case 2:
        EXPECT_EQ(pt, pt3);
        break;
      default:
        break;
    }

    ++nbObjectsRead;
  }
  EXPECT_EQ(nbObjectsRead, 3);
}
}  // namespace cct
