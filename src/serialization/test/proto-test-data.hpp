#pragma once

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

#include "market.hpp"
#include "monetaryamount.hpp"
#include "proto-public-trade-converter.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class ProtobufBaseDataTest : public ::testing::Test {
 protected:
  TimePoint tp1{milliseconds{std::numeric_limits<int64_t>::max() / 10000000}};
  TimePoint tp2{milliseconds{std::numeric_limits<int64_t>::max() / 9000000}};
  TimePoint tp3{milliseconds{std::numeric_limits<int64_t>::max() / 8000000}};
  TimePoint tp4{milliseconds{std::numeric_limits<int64_t>::max() / 7000000}};
  TimePoint tp5{milliseconds{std::numeric_limits<int64_t>::max() / 6900000}};
  TimePoint tp6{milliseconds{std::numeric_limits<int64_t>::max() / 6800000}};
  TimePoint tp7{milliseconds{std::numeric_limits<int64_t>::max() / 6700000}};
  TimePoint tp8{milliseconds{std::numeric_limits<int64_t>::max() / 6600000}};
  TimePoint tp9{milliseconds{std::numeric_limits<int64_t>::max() / 6500000}};
  TimePoint tp10 = tp9 + milliseconds{1};

  TimeWindow timeWindowAll{tp1, tp10 + milliseconds{1}};
  TimeWindow timeWindow14{tp1, tp5};
  TimeWindow timeWindow79{tp7, tp9 + milliseconds{1}};

  Market mk1{"ETH", "USDT"};
  Market mk2{"BTC", "USD"};
  Market mk3{"SHIB", "USDT"};
  Market mk4{"SOL", "BTC"};
  Market mk5{"SOL", "ETH"};
  Market mk6{"ETH", "BTC"};
  Market mk7{"DOGE", "CAD"};

  PublicTrade pt1{TradeSide::kBuy, MonetaryAmount{"0.13", mk1.base()}, MonetaryAmount{"1500.5", mk1.quote()}, tp1};
  PublicTrade pt2{TradeSide::kSell, MonetaryAmount{"3.7", mk1.base()}, MonetaryAmount{"1500.5", mk1.quote()}, tp2};
  PublicTrade pt3{TradeSide::kBuy, MonetaryAmount{"0.004", mk1.base()}, MonetaryAmount{1501, mk1.quote()}, tp3};
  PublicTrade pt4{TradeSide::kBuy, MonetaryAmount{"44473434", mk3.base()}, MonetaryAmount{"0.00045", mk3.quote()}, tp1};
  PublicTrade pt5{TradeSide::kBuy, MonetaryAmount{"45.0986", mk4.base()}, MonetaryAmount{"0.00045", mk4.quote()}, tp7};
  PublicTrade pt6{TradeSide::kSell, MonetaryAmount{"0.81153", mk6.base()}, MonetaryAmount{"0.0834", mk6.quote()}, tp6};
  PublicTrade pt7{TradeSide::kSell, MonetaryAmount{694873, mk7.base()}, MonetaryAmount{"0.045", mk7.quote()}, tp8};
  PublicTrade pt8{TradeSide::kSell, MonetaryAmount{"0.1", mk2.base()}, MonetaryAmount{50000, mk2.quote()}, tp4};
  PublicTrade pt9{TradeSide::kSell, MonetaryAmount{"56", mk1.base()}, MonetaryAmount{1300, mk1.quote()}, tp5};
  PublicTrade pt10{TradeSide::kBuy, MonetaryAmount{"37.8", mk5.base()}, MonetaryAmount{"0.032", mk5.quote()}, tp9};
  PublicTrade pt11{TradeSide::kBuy, MonetaryAmount{"12.2", mk5.base()}, MonetaryAmount{"0.033", mk5.quote()}, tp10};

  ::proto::PublicTrade td1{ConvertPublicTradeToProto(pt1)};
  ::proto::PublicTrade td2{ConvertPublicTradeToProto(pt2)};
  ::proto::PublicTrade td3{ConvertPublicTradeToProto(pt3)};
  ::proto::PublicTrade td4{ConvertPublicTradeToProto(pt4)};
  ::proto::PublicTrade td5{ConvertPublicTradeToProto(pt5)};
  ::proto::PublicTrade td6{ConvertPublicTradeToProto(pt6)};
  ::proto::PublicTrade td7{ConvertPublicTradeToProto(pt7)};
  ::proto::PublicTrade td8{ConvertPublicTradeToProto(pt8)};
  ::proto::PublicTrade td9{ConvertPublicTradeToProto(pt9)};
  ::proto::PublicTrade td10{ConvertPublicTradeToProto(pt10)};
  ::proto::PublicTrade td11{ConvertPublicTradeToProto(pt11)};
};

}  // namespace cct