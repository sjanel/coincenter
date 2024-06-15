#include "proto-market-order-book-converter.hpp"

#include <gtest/gtest.h>

#include <initializer_list>

#include "amount-price.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "order-book-line.hpp"
#include "timedef.hpp"

namespace cct {

// TODO: factorize duplicated code from marketorderbook_test
namespace {
using AmountAtPriceVec = MarketOrderBook::AmountPerPriceVec;

MarketOrderBookLines CreateMarketOrderBookLines(std::initializer_list<OrderBookLine> init) {
  MarketOrderBookLines marketOrderBookLines;
  marketOrderBookLines.reserve(init.size());

  for (const auto &orderBookLine : init) {
    if (orderBookLine.amount() < 0) {
      marketOrderBookLines.pushAsk(-orderBookLine.amount(), orderBookLine.price());
    } else {
      marketOrderBookLines.pushBid(orderBookLine.amount(), orderBookLine.price());
    }
  }

  return marketOrderBookLines;
}

}  // namespace

class ProtoMarketOrderBookTest : public ::testing::Test {
 protected:
  TimePoint time;
  Market market{"APM", "KRW"};
  MarketOrderBook marketOrderBook{
      time, market,
      CreateMarketOrderBookLines(
          {OrderBookLine(MonetaryAmount("1991.3922", "APM"), MonetaryAmount("57.8", "KRW"), OrderBookLine::Type::kAsk),
           OrderBookLine(MonetaryAmount("90184.3951", "APM"), MonetaryAmount("57.81", "KRW"),
                         OrderBookLine::Type::kAsk),
           OrderBookLine(MonetaryAmount("91.1713", "APM"), MonetaryAmount("57.84", "KRW"), OrderBookLine::Type::kAsk),
           OrderBookLine(MonetaryAmount("41.0131", "APM"), MonetaryAmount("57.9", "KRW"), OrderBookLine::Type::kAsk),
           OrderBookLine(MonetaryAmount("33.5081914157147802", "APM"), MonetaryAmount("57.78", "KRW"),
                         OrderBookLine::Type::kAsk),
           OrderBookLine(MonetaryAmount("3890.879", "APM"), MonetaryAmount("57.19", "KRW"), OrderBookLine::Type::kBid),
           OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.18", "KRW"), OrderBookLine::Type::kBid),
           OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.17", "KRW"), OrderBookLine::Type::kBid),
           OrderBookLine(MonetaryAmount("3848.8453", "APM"), MonetaryAmount("57.16", "KRW"),
                         OrderBookLine::Type::kBid)})};

  MarketOrderBookConverter marketOrderBookConverter{market};
};

TEST_F(ProtoMarketOrderBookTest, Serialization) {
  const auto protoObj = ConvertMarketOrderBookToProto(marketOrderBook);
  const auto [volNbDecimals, priNbDecimals] = marketOrderBook.volAndPriNbDecimals();

  EXPECT_EQ(TimePoint{milliseconds{protoObj.unixtimestampinms()}}, marketOrderBook.time());
  EXPECT_EQ(protoObj.volumenbdecimals(), volNbDecimals);
  EXPECT_EQ(protoObj.pricenbdecimals(), priNbDecimals);

  ASSERT_TRUE(protoObj.has_orderbook());
  ASSERT_EQ(protoObj.orderbook().asks_size(), 5U);

  const auto &asks = protoObj.orderbook().asks();

  EXPECT_EQ(asks[0].volume(), 335081914157147);
  EXPECT_EQ(asks[0].price(), 577800000000000000);

  EXPECT_EQ(asks[1].volume(), 19913922000000000);
  EXPECT_EQ(asks[1].price(), 578000000000000000);

  EXPECT_EQ(asks[2].volume(), 901843951000000000);
  EXPECT_EQ(asks[2].price(), 578100000000000000);

  EXPECT_EQ(asks[3].volume(), 911713000000000);
  EXPECT_EQ(asks[3].price(), 578400000000000000);

  EXPECT_EQ(asks[4].volume(), 410131000000000);
  EXPECT_EQ(asks[4].price(), 579000000000000000);

  ASSERT_EQ(protoObj.orderbook().bids_size(), 4U);

  const auto &bids = protoObj.orderbook().bids();

  EXPECT_EQ(bids[0].volume(), 38908790000000000);
  EXPECT_EQ(bids[0].price(), 571900000000000000);

  EXPECT_EQ(bids[1].volume(), 140000000000000);
  EXPECT_EQ(bids[1].price(), 571800000000000000);

  EXPECT_EQ(bids[2].volume(), 140000000000000);
  EXPECT_EQ(bids[2].price(), 571700000000000000);

  EXPECT_EQ(bids[3].volume(), 38488453000000000);
  EXPECT_EQ(bids[3].price(), 571600000000000000);
}

TEST_F(ProtoMarketOrderBookTest, SerializeThenDeserializeShouldGiveSameObject) {
  const auto protoObj = ConvertMarketOrderBookToProto(marketOrderBook);

  const auto marketOrderBookConvertedBack = marketOrderBookConverter(protoObj);

  EXPECT_TRUE(marketOrderBookConvertedBack.isValid());

  EXPECT_EQ(marketOrderBook, marketOrderBookConvertedBack);
}
}  // namespace cct