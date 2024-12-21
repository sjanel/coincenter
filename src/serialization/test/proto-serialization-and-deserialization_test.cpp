#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iterator>
#include <map>
#include <string_view>
#include <thread>

#include "market-timestamp-set.hpp"
#include "market-timestamp.hpp"
#include "monetaryamount.hpp"
#include "proto-deserializer.hpp"
#include "proto-public-trade-compare.hpp"
#include "proto-public-trade-converter.hpp"
#include "proto-serializer.hpp"
#include "proto-test-data.hpp"
#include "public-trade-vector.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"
#include "serialization-tools.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

namespace {
constexpr auto RandStr() {
  std::array<char, 10UL> str;

  std::ranges::generate(str, []() {
    static constexpr char kCharSet[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
    static constexpr size_t kMaxIdx = (sizeof(kCharSet) - 1);
    return kCharSet[std::rand() % kMaxIdx];
  });

  return str;
}
}  // namespace

class ProtobufSerializerDeserializerTest : public ProtobufBaseDataTest {
 protected:
  void TearDown() override {
    std::filesystem::remove_all(subPath1);
    std::filesystem::remove_all(subPath2);
  }

  std::filesystem::path subPath1 = std::filesystem::temp_directory_path() / std::string_view(RandStr());
  std::filesystem::path subPath2 = std::filesystem::temp_directory_path() / std::string_view(RandStr());

  static constexpr int32_t kSmallRehashThreshold = 3;

  using DurationTypeFlush = milliseconds;
  static constexpr int32_t kFlushNbMillis = 1;

  int32_t nbTradesPerMarketInMemory{100};

  using Serializer = ProtobufObjectsSerializer<::proto::PublicTrade, ProtoPublicTradeComp, ProtoPublicTradeEqual,
                                               kSmallRehashThreshold, DurationTypeFlush, kFlushNbMillis>;
  using Deserializer = ProtobufObjectsDeserializer<::proto::PublicTrade, PublicTradeConverter>;

  Serializer createSerializer(const MarketTimestampSet &marketTimestampSet = MarketTimestampSet{}) {
    return Serializer{subPath1, marketTimestampSet, nbTradesPerMarketInMemory};
  }
  Deserializer createDeserializer() { return Deserializer{subPath1}; }

  void serializeSomeObjects(Serializer &serializer) {
    // push two times same object (should not be duplicated during writing)
    serializer.push(mk1, td2);
    serializer.push(mk1, td2);

    // Even if older object, should be pushed as well as serializer has not written any objects yet (they should be
    // ordered internally before writes)
    serializer.push(mk1, td1);

    serializer.push(mk1, td3);
    serializer.push(mk3, td4);
    serializer.push(mk4, td5);

    serializer.push(mk1, td9);

    // To force a write and make sure that serializer writes in append mode on a same file
    for (decltype(nbTradesPerMarketInMemory) pushPos = 0; pushPos < nbTradesPerMarketInMemory; ++pushPos) {
      serializer.push(mk5, td10);
    }

    std::this_thread::sleep_for(milliseconds{2});

    serializer.push(mk5, td11);

    serializer.push(mk7, td7);

    Serializer anotherSerializer{subPath2, MarketTimestampSet{}, nbTradesPerMarketInMemory};

    serializer.swap(anotherSerializer);

    // Should not be pushed to 'subPath'
    serializer.push(mk6, td6);
  }
};

TEST_F(ProtobufSerializerDeserializerTest, SerializeThenDeserializeSomeObjects) {
  {
    auto serializer = createSerializer();
    serializeSomeObjects(serializer);
  }

  // make sure serializer writes all at destruction

  const std::filesystem::path kExpectedFiles[] = {
      subPath2 / std::string_view{mk6.str()} / "2012" / "12" / "24" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk7.str()} / "2014" / "04" / "14" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "2006" / "07" / "14" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "2012" / "05" / "11" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk3.str()} / "1999" / "03" / "25" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "2013" / "08" / "16" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk5.str()} / "2014" / "12" / "19" / ComputeProtoFileName(9)};

  auto isFilePresent = [](const auto &fileName) { return std::filesystem::exists(fileName); };

  EXPECT_TRUE(std::ranges::all_of(kExpectedFiles, isFilePresent));

  auto deserializer = createDeserializer();
  auto marketTimestampSet = deserializer.listMarkets(timeWindowAll);

  EXPECT_EQ(marketTimestampSet,
            MarketTimestampSet({MarketTimestamp(mk7, tp8), MarketTimestamp(mk1, tp5), MarketTimestamp(mk3, tp1),
                                MarketTimestamp(mk4, tp7), MarketTimestamp(mk5, tp10)}));

  // Should not serialize again as timestamps are set to last written
  {
    auto serializer = createSerializer(marketTimestampSet);
    serializeSomeObjects(serializer);
  }

  EXPECT_TRUE(deserializer.loadMarket(mk1, timeWindow79).empty());
  EXPECT_EQ(deserializer.loadMarket(mk1, timeWindowAll), vector<PublicTrade>({pt1, pt2, pt3, pt9}));
  EXPECT_EQ(deserializer.loadMarket(mk4, timeWindowAll), vector<PublicTrade>({pt5}));
  EXPECT_TRUE(deserializer.loadMarket(mk4, timeWindow14).empty());
  EXPECT_EQ(deserializer.loadMarket(mk7, timeWindowAll), vector<PublicTrade>({pt7}));
  EXPECT_EQ(deserializer.loadMarket(mk7, timeWindow79), vector<PublicTrade>({pt7}));
  EXPECT_TRUE(deserializer.loadMarket(Market{"UNK", "OTH"}, timeWindowAll).empty());
  EXPECT_EQ(deserializer.loadMarket(mk5, timeWindowAll), vector<PublicTrade>({pt10, pt11}));
}

TEST_F(ProtobufSerializerDeserializerTest, ManySerializationsDifferentHoursOfDay) {
  static const TimePoint kTimePoints[] = {tp1, tp2};
  static const Market kMarkets[] = {mk1, mk4};
  static constexpr Duration kDurationRange = std::chrono::weeks(2);
  static constexpr Duration kDurationStep = std::chrono::minutes(199);

  std::map<Market, PublicTradeVector> pushedPublicTrades;

  {
    auto serializer = createSerializer();

    for (Market market : kMarkets) {
      for (TimePoint tp : kTimePoints) {
        for (auto ts = tp; ts < tp + kDurationRange; ts += kDurationStep) {
          TradeSide side = tp == tp1 ? TradeSide::buy : TradeSide::sell;
          MonetaryAmount amount{"0.13", market.base()};
          MonetaryAmount price{"1500.5", market.quote()};
          PublicTrade pt{side, amount, price, ts};

          pushedPublicTrades[market].push_back(pt);
          serializer.push(market, ConvertPublicTradeToProto(pt));
        }

        std::this_thread::sleep_for(milliseconds{2});
      }
    }
  }

  const std::filesystem::path kExpectedFiles[] = {
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "25" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "26" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "27" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "28" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "29" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "30" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "1999" / "03" / "31" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "01" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "02" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "03" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "04" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "05" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "06" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "07" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "08" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "1999" / "04" / "08" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "23" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "24" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "25" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "26" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "27" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "28" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "29" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "2002" / "06" / "30" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "01" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "02" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "03" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "04" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "05" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "06" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "07" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "07" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk1.str()} / "2002" / "07" / "07" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "25" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "26" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "27" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "28" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "29" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "30" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "1999" / "03" / "31" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "01" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "02" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "03" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "04" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "05" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "06" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "07" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "08" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "1999" / "04" / "08" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "23" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "23" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "23" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "23" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "23" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "24" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "25" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "26" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "27" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "28" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "29" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "2002" / "06" / "30" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "01" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "02" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(6),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(10),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(13),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(16),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(20),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "03" / ComputeProtoFileName(23),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(2),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(9),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(12),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(19),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "04" / ComputeProtoFileName(22),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(5),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(8),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(15),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(18),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "05" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(1),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(4),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(7),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(11),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(14),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(17),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "06" / ComputeProtoFileName(21),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "07" / ComputeProtoFileName(0),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "07" / ComputeProtoFileName(3),
      subPath1 / std::string_view{mk4.str()} / "2002" / "07" / "07" / ComputeProtoFileName(6),
  };

  auto isFilePresent = [](const auto &fileName) { return std::filesystem::exists(fileName); };

  EXPECT_TRUE(std::ranges::all_of(kExpectedFiles, isFilePresent));

  auto deserializer = createDeserializer();
  auto marketTimestampSet = deserializer.listMarkets(timeWindowAll);

  ASSERT_EQ(marketTimestampSet.size(), 2U);

  const auto &marketTimestamp = marketTimestampSet.front();

  const auto lastTp = *std::next(std::end(kTimePoints), -1);

  EXPECT_EQ(marketTimestamp.market, mk1);
  EXPECT_GT(marketTimestamp.timePoint + kDurationStep, lastTp + kDurationRange);

  for (Market market : kMarkets) {
    auto allData = deserializer.loadMarket(market, timeWindowAll);

    EXPECT_EQ(pushedPublicTrades[market], allData);

    auto partialData =
        deserializer.loadMarket(market, TimeWindow{kTimePoints[0], kTimePoints[0] + std::chrono::days(1)});

    // below check is an assert because we should not launch the std::equal below if this condition is not satisfied
    ASSERT_LT(partialData.size(), allData.size());

    EXPECT_EQ(partialData.size(), 8U);
    EXPECT_TRUE(
        std::equal(partialData.begin(), partialData.end(), allData.begin(), allData.begin() + partialData.size()));
  }
}

}  // namespace cct