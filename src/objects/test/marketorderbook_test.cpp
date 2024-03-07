#include "marketorderbook.hpp"

#include <gtest/gtest.h>

#include <array>
#include <optional>
#include <utility>

#include "cct_exception.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {
namespace {
using AmountAtPrice = MarketOrderBook::AmountAtPrice;
using AmountAtPriceVec = MarketOrderBook::AmountPerPriceVec;
}  // namespace

inline bool operator==(const AmountAtPrice &lhs, const AmountAtPrice &rhs) {
  return lhs.amount == rhs.amount && lhs.price == rhs.price;
}

TEST(MarketOrderBookTest, Basic) { EXPECT_TRUE(MarketOrderBook(Clock::now(), Market("ETH", "EUR"), {}).empty()); }

class MarketOrderBookTestCase1 : public ::testing::Test {
 protected:
  MarketOrderBook marketOrderBook{
      Clock::now(), Market("ETH", "EUR"),
      std::array<OrderBookLine, 6>{
          OrderBookLine(MonetaryAmount("0.65", "ETH"), MonetaryAmount("1300.50", "EUR"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount("0.24", "ETH"), MonetaryAmount("1301", "EUR"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount(0, "ETH"), MonetaryAmount("1301.50", "EUR"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount("1.4009", "ETH"), MonetaryAmount("1302", "EUR"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("3.78", "ETH"), MonetaryAmount("1302.50", "EUR"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("56.10001267", "ETH"), MonetaryAmount("1303", "EUR"),
                        OrderBookLine::Type::kAsk)}};
};

TEST_F(MarketOrderBookTestCase1, NumberOfElements) {
  EXPECT_EQ(marketOrderBook.size(), 5);
  EXPECT_EQ(marketOrderBook.nbAskPrices(), 3);
  EXPECT_EQ(marketOrderBook.nbBidPrices(), 2);
}

TEST_F(MarketOrderBookTestCase1, MiddleElements) {
  EXPECT_EQ(marketOrderBook.lowestAskPrice(), MonetaryAmount("1302", "EUR"));
  EXPECT_EQ(marketOrderBook.highestBidPrice(), MonetaryAmount("1301", "EUR"));
}

TEST_F(MarketOrderBookTestCase1, OperatorBrackets) {
  EXPECT_EQ(marketOrderBook[-2], std::make_pair(MonetaryAmount("1300.5EUR"), MonetaryAmount("0.65ETH")));
  EXPECT_EQ(marketOrderBook[-1], std::make_pair(MonetaryAmount("1301EUR"), MonetaryAmount("0.24ETH")));
  EXPECT_EQ(marketOrderBook[0], std::make_pair(MonetaryAmount("1301.5EUR"), MonetaryAmount("0.82045ETH")));
  EXPECT_EQ(marketOrderBook[1], std::make_pair(MonetaryAmount("1302EUR"), MonetaryAmount("1.4009ETH")));
  EXPECT_EQ(marketOrderBook[2], std::make_pair(MonetaryAmount("1302.5EUR"), MonetaryAmount("3.78ETH")));
  EXPECT_EQ(marketOrderBook[3], std::make_pair(MonetaryAmount("1303EUR"), MonetaryAmount("56.10001267ETH")));
}

TEST_F(MarketOrderBookTestCase1, ComputeCumulAmountBoughtImmediately) {
  EXPECT_EQ(marketOrderBook.computeCumulAmountBoughtImmediatelyAt(MonetaryAmount("1302.25", "EUR")),
            MonetaryAmount("1.4009", "ETH"));
  EXPECT_EQ(marketOrderBook.computeCumulAmountBoughtImmediatelyAt(MonetaryAmount("1302.5", "EUR")),
            MonetaryAmount("5.1809", "ETH"));
  EXPECT_EQ(marketOrderBook.computeCumulAmountBoughtImmediatelyAt(MonetaryAmount("1300.75", "EUR")),
            MonetaryAmount(0, "ETH"));
  EXPECT_THROW(marketOrderBook.computeCumulAmountBoughtImmediatelyAt(MonetaryAmount(1, "ETH")), exception);
}

TEST_F(MarketOrderBookTestCase1, ComputeCumulAmountSoldImmediately) {
  EXPECT_EQ(marketOrderBook.computeCumulAmountSoldImmediatelyAt(MonetaryAmount("1301", "EUR")),
            MonetaryAmount("0.24", "ETH"));
  EXPECT_EQ(marketOrderBook.computeCumulAmountSoldImmediatelyAt(MonetaryAmount(1, "EUR")),
            MonetaryAmount("0.89", "ETH"));
  EXPECT_EQ(marketOrderBook.computeCumulAmountSoldImmediatelyAt(MonetaryAmount("1303.5", "EUR")),
            MonetaryAmount(0, "ETH"));
  EXPECT_THROW(marketOrderBook.computeCumulAmountSoldImmediatelyAt(MonetaryAmount(1, "ETH")), exception);
}

TEST_F(MarketOrderBookTestCase1, ComputeMinPriceAtWhichAmountWouldBeBoughtImmediately) {
  EXPECT_EQ(marketOrderBook.computeMinPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount(0, "ETH")),
            MonetaryAmount("1301", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMinPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount("0.1", "ETH")),
            MonetaryAmount("1301", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMinPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount("0.3", "ETH")),
            MonetaryAmount("1300.5", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMinPriceAtWhichAmountWouldBeSoldImmediately(MonetaryAmount(1, "ETH")), std::nullopt);
}

TEST_F(MarketOrderBookTestCase1, ComputeMaxPriceAtWhichAmountWouldBeBoughtImmediately) {
  EXPECT_EQ(marketOrderBook.computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount(0, "ETH")),
            MonetaryAmount("1302", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount(1, "ETH")),
            MonetaryAmount("1302", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount(10, "ETH")),
            MonetaryAmount("1303", "EUR"));
  EXPECT_EQ(marketOrderBook.computeMaxPriceAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount("100", "ETH")),
            std::nullopt);
}

TEST_F(MarketOrderBookTestCase1, ComputeAvgPriceForTakerBuy) {
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(1000, "EUR")),
            AmountAtPrice(MonetaryAmount("999.99999999998784", "EUR"), MonetaryAmount("1302.00000000000001", "EUR")));
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(5000, "EUR")),
            AmountAtPrice(MonetaryAmount("4999.9999119826894", "EUR"), MonetaryAmount("1302.31755833325309", "EUR")));
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(100000, "EUR")),
            AmountAtPrice(MonetaryAmount("79845.737428463776", "EUR"), MonetaryAmount("1302.94629812356546", "EUR")));
}

TEST_F(MarketOrderBookTestCase1, ComputeAvgPriceForTakerSell) {
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(24, "ETH", 2)),
            AmountAtPrice(MonetaryAmount(24, "ETH", 2), MonetaryAmount(1301, "EUR")));
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(5, "ETH", 1)),
            AmountAtPrice(MonetaryAmount(5, "ETH", 1), MonetaryAmount(130074, "EUR", 2)));
  EXPECT_EQ(marketOrderBook.avgPriceAndMatchedAmountTaker(MonetaryAmount(4, "ETH")),
            AmountAtPrice(MonetaryAmount(89, "ETH", 2), MonetaryAmount("1300.63483146067415", "EUR")));
}

TEST_F(MarketOrderBookTestCase1, MoreComplexListOfPricesComputations) {
  EXPECT_EQ(marketOrderBook.computePricesAtWhichAmountWouldBeBoughtImmediately(MonetaryAmount(4, "ETH")),
            AmountAtPriceVec({AmountAtPrice(MonetaryAmount("1.4009", "ETH"), MonetaryAmount("1302", "EUR")),
                              AmountAtPrice(MonetaryAmount("2.5991", "ETH"), MonetaryAmount("1302.50", "EUR"))}));
  EXPECT_EQ(marketOrderBook.computePricesAtWhichAmountWouldBeSoldImmediately(MonetaryAmount("0.24", "ETH")),
            AmountAtPriceVec({AmountAtPrice(MonetaryAmount("0.24", "ETH"), MonetaryAmount("1301", "EUR"))}));
}

TEST_F(MarketOrderBookTestCase1, ConvertBaseAmountToQuote) {
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("0.56", "ETH")), MonetaryAmount("728.4", "EUR"));
}

TEST_F(MarketOrderBookTestCase1, ConvertQuoteAmountToBase) {
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("800", "EUR")), MonetaryAmount("0.61443932411674347", "ETH"));
}

TEST_F(MarketOrderBookTestCase1, Convert) {
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("0.56", "ETH")), MonetaryAmount("728.4", "EUR"));
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("800", "EUR")), MonetaryAmount("0.61443932411674347", "ETH"));
}

class MarketOrderBookTestCase2 : public ::testing::Test {
 protected:
  TimePoint time{};
  MarketOrderBook marketOrderBook{
      time, Market("APM", "KRW"),
      std::array<OrderBookLine, 9>{
          OrderBookLine(MonetaryAmount("1991.3922", "APM"), MonetaryAmount("57.8", "KRW"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("90184.3951", "APM"), MonetaryAmount("57.81", "KRW"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("91.1713", "APM"), MonetaryAmount("57.84", "KRW"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("41.0131", "APM"), MonetaryAmount("57.9", "KRW"), OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("33.5081914157147802", "APM"), MonetaryAmount("57.78", "KRW"),
                        OrderBookLine::Type::kAsk),
          OrderBookLine(MonetaryAmount("3890.879", "APM"), MonetaryAmount("57.19", "KRW"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.18", "KRW"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.17", "KRW"), OrderBookLine::Type::kBid),
          OrderBookLine(MonetaryAmount("3848.8453", "APM"), MonetaryAmount("57.16", "KRW"),
                        OrderBookLine::Type::kBid)}};
};

TEST_F(MarketOrderBookTestCase2, SimpleQueries) {
  EXPECT_EQ(marketOrderBook.size(), 9);
  EXPECT_EQ(marketOrderBook.lowestAskPrice(), MonetaryAmount("57.78", "KRW"));
  EXPECT_EQ(marketOrderBook.highestBidPrice(), MonetaryAmount("57.19", "KRW"));
}

TEST_F(MarketOrderBookTestCase2, ConvertQuoteAmountToBase) {
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("50000000", "KRW")), std::optional<MonetaryAmount>());
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("500", "KRW")), MonetaryAmount("8.6535133264105226", "APM"));
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("500000", "KRW")), MonetaryAmount("8649.3845211510554", "APM"));
}

TEST_F(MarketOrderBookTestCase2, ComputeMatchedPartsBuy) {
  EXPECT_EQ(
      marketOrderBook.computeMatchedParts(TradeSide::kBuy, MonetaryAmount(91000, "APM"),
                                          MonetaryAmount("57.81", "KRW")),
      AmountAtPriceVec({AmountAtPrice(MonetaryAmount("33.5081914157147", "APM"), MonetaryAmount("57.78", "KRW")),
                        AmountAtPrice(MonetaryAmount("1991.3922", "APM"), MonetaryAmount("57.8", "KRW")),
                        AmountAtPrice(MonetaryAmount("88975.0996085842853", "APM"), MonetaryAmount("57.81", "KRW"))}));
  EXPECT_EQ(marketOrderBook.computeMatchedParts(TradeSide::kBuy, MonetaryAmount(91000, "APM"),
                                                MonetaryAmount("57.77", "KRW")),
            AmountAtPriceVec());
}

TEST_F(MarketOrderBookTestCase2, ComputeMatchedPartsSell) {
  EXPECT_EQ(marketOrderBook.computeMatchedParts(TradeSide::kSell, MonetaryAmount(5000, "APM"),
                                                MonetaryAmount("57.19", "KRW")),
            AmountAtPriceVec({
                AmountAtPrice(MonetaryAmount("3890.879", "APM"), MonetaryAmount("57.19", "KRW")),
            }));
  EXPECT_EQ(marketOrderBook.computeMatchedParts(TradeSide::kSell, MonetaryAmount(91000, "APM"),
                                                MonetaryAmount("57.23", "KRW")),
            AmountAtPriceVec());
}

class MarketOrderBookTestCase3 : public ::testing::Test {
 protected:
  TimePoint time{};
  MarketOrderBook marketOrderBook{
      time, Market("XLM", "BTC"),
      std::array<OrderBookLine, 6>{OrderBookLine(MonetaryAmount("126881.164", "XLM"),
                                                 MonetaryAmount("0.000007130", "BTC"), OrderBookLine::Type::kAsk),
                                   OrderBookLine(MonetaryAmount("95716.519", "XLM"),
                                                 MonetaryAmount("0.000007120", "BTC"), OrderBookLine::Type::kAsk),
                                   OrderBookLine(MonetaryAmount("23726.285", "XLM"),
                                                 MonetaryAmount("0.000007110", "BTC"), OrderBookLine::Type::kAsk),
                                   OrderBookLine(MonetaryAmount("37863.710", "XLM"),
                                                 MonetaryAmount("0.000007100", "BTC"), OrderBookLine::Type::kBid),
                                   OrderBookLine(MonetaryAmount("169165.594", "XLM"),
                                                 MonetaryAmount("0.000007090", "BTC"), OrderBookLine::Type::kBid),
                                   OrderBookLine(MonetaryAmount("204218.966", "XLM"),
                                                 MonetaryAmount("0.000007080", "BTC"), OrderBookLine::Type::kBid)}};
};

TEST_F(MarketOrderBookTestCase3, Convert) {
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("600000", "XLM")), std::nullopt);
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount(3, "BTC")), std::nullopt);
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("42050", "XLM")), MonetaryAmount("0.2985131371", "BTC"));
  EXPECT_EQ(marketOrderBook.convert(MonetaryAmount("1.5405478119", "BTC")),
            MonetaryAmount("216266.409928471248", "XLM"));
}

class MarketOrderBookTestCaseExtended1 : public ::testing::Test {
 protected:
  TimePoint time{};
  MarketOrderBook marketOrderBook{time,
                                  MonetaryAmount("2300.45 EUR"),
                                  MonetaryAmount("193.09 ADA"),
                                  MonetaryAmount("2300.4 EUR"),
                                  MonetaryAmount("41 ADA"),
                                  {2, 2},
                                  50};
};

TEST_F(MarketOrderBookTestCaseExtended1, LimitPrice) {
  EXPECT_EQ(marketOrderBook.highestBidPrice(), MonetaryAmount("2300.4 EUR"));
  EXPECT_EQ(marketOrderBook.lowestAskPrice(), MonetaryAmount("2300.45 EUR"));
}

TEST_F(MarketOrderBookTestCaseExtended1, Convert) {
  EXPECT_NE(marketOrderBook.convert(MonetaryAmount("10000 EUR")), std::nullopt);
  EXPECT_NE(marketOrderBook.convert(MonetaryAmount("10000 ADA")), std::nullopt);
}

TEST(MarketOrderBookExtendedTest, ComputeVolAndPriNbDecimalsFromTickerInfo) {
  MarketOrderBook marketOrderBook(Clock::now(), MonetaryAmount("12355.00002487 XLM"),
                                  MonetaryAmount("193.0900000000078 ADA"), MonetaryAmount("12355.00002486 XLM"),
                                  MonetaryAmount("504787104.7801 ADA"), {4, 8}, 10);

  EXPECT_EQ(marketOrderBook.highestBidPrice(), MonetaryAmount("12355.00002486 XLM"));
  EXPECT_EQ(marketOrderBook.lowestAskPrice(), MonetaryAmount("12355.00002487 XLM"));
}

TEST(MarketOrderBookExtendedTest, InvalidPrice) {
  EXPECT_NO_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                                  MonetaryAmount("5ADA"), {0, 0}));
  EXPECT_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("1XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("5ADA"), {0, 0}),
               exception);
}

TEST(MarketOrderBookExtendedTest, InvalidDepth) {
  EXPECT_NO_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                                  MonetaryAmount("5ADA"), {0, 0}));
  EXPECT_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("5ADA"), {0, 0}, 0),
               exception);
  EXPECT_NO_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                                  MonetaryAmount("5ADA"), {2, 2}));
  EXPECT_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("5ADA"), {2, 2}, -1),
               exception);
}

TEST(MarketOrderBookExtendedTest, InvalidNumberOfDecimals) {
  EXPECT_NO_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("0.03XLM"), MonetaryAmount("1ADA"),
                                  MonetaryAmount("0.02XLM"), MonetaryAmount("5ADA"), {8, 8}));
  EXPECT_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("0.03XLM"), MonetaryAmount("1ADA"),
                               MonetaryAmount("0.02XLM"), MonetaryAmount("5ADA"), {8, 1}),
               exception);
  EXPECT_NO_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("0.04ADA"),
                                  MonetaryAmount("1XLM"), MonetaryAmount("0.03ADA"), {8, 8}));
  EXPECT_THROW(MarketOrderBook(Clock::now(), MonetaryAmount("2XLM"), MonetaryAmount("0.04ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("0.03ADA"), {1, 8}),
               exception);
}

}  // namespace cct
