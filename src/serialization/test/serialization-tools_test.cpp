#include "serialization-tools.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(SerializationTools, ComputeProtoSubPath) {
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "upbit", "order-books"), "/path/to/data/serialized/order-books/upbit");
  EXPECT_EQ(ComputeProtoSubPath("another-path", "bithumb", "trades"), "another-path/serialized/trades/bithumb");
  EXPECT_EQ(ComputeProtoSubPath(".", "kraken", "trades"), "./serialized/trades/kraken");
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "binance", "order-books"),
            "/path/to/data/serialized/order-books/binance");
  EXPECT_EQ(ComputeProtoSubPath("/path/to/data", "kucoin", "trades"), "/path/to/data/serialized/trades/kucoin");
}

TEST(SerializationTools, MonthStr) {
  EXPECT_EQ(MonthStr(1), "01");
  EXPECT_EQ(MonthStr(6), "06");
  EXPECT_EQ(MonthStr(10), "10");
  EXPECT_EQ(MonthStr(11), "11");
  EXPECT_EQ(MonthStr(12), "12");
}

TEST(SerializationTools, DayOfMonthStr) {
  EXPECT_EQ(DayOfMonthStr(1), "01");
  EXPECT_EQ(DayOfMonthStr(2), "02");
  EXPECT_EQ(DayOfMonthStr(10), "10");
  EXPECT_EQ(DayOfMonthStr(17), "17");
  EXPECT_EQ(DayOfMonthStr(22), "22");
  EXPECT_EQ(DayOfMonthStr(31), "31");
}

TEST(SerializationTools, ComputeProtoFileName) {
  EXPECT_EQ(ComputeProtoFileName(0), "00-00-00_00-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(4), "04-00-00_04-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(17), "17-00-00_17-59-59.binpb");
  EXPECT_EQ(ComputeProtoFileName(23), "23-00-00_23-59-59.binpb");
}

}  // namespace cct