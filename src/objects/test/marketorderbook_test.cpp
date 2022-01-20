#include "marketorderbook.hpp"

#include <gtest/gtest.h>

#include "cct_exception.hpp"

namespace cct {
namespace {
using AmountAtPrice = MarketOrderBook::AmountAtPrice;
using AmountAtPriceVec = MarketOrderBook::AmountPerPriceVec;
}  // namespace

inline bool operator==(const AmountAtPrice &lhs, const AmountAtPrice &rhs) {
  return lhs.amount == rhs.amount && lhs.price == rhs.price;
}

TEST(MarketOrderBookTest, Basic) {
  MarketOrderBook marketOrderBook(Market("ETH", "EUR"), {});
  EXPECT_TRUE(marketOrderBook.empty());
}

class MarketOrderBookTestCase1 : public ::testing::Test {
 protected:
  MarketOrderBookTestCase1()
      : marketOrderBook(Market("ETH", "EUR"),
                        std::array<OrderBookLine, 6>{
                            OrderBookLine(MonetaryAmount("0.65", "ETH"), MonetaryAmount("1300.50", "EUR"), false),
                            OrderBookLine(MonetaryAmount("0.24", "ETH"), MonetaryAmount("1301", "EUR"), false),
                            OrderBookLine(MonetaryAmount(0, "ETH"), MonetaryAmount("1301.50", "EUR"), false),
                            OrderBookLine(MonetaryAmount("1.4009", "ETH"), MonetaryAmount("1302", "EUR"), true),
                            OrderBookLine(MonetaryAmount("3.78", "ETH"), MonetaryAmount("1302.50", "EUR"), true),
                            OrderBookLine(MonetaryAmount("56.10001267", "ETH"), MonetaryAmount("1303", "EUR"), true)}) {
  }
  virtual void SetUp() {}
  virtual void TearDown() {}

  MarketOrderBook marketOrderBook;
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

TEST_F(MarketOrderBookTestCase1, ComputeAvgPriceForTakerAmount) {
  EXPECT_EQ(marketOrderBook.computeAvgPriceForTakerAmount(MonetaryAmount(4, "ETH")), std::nullopt);
  EXPECT_EQ(marketOrderBook.computeAvgPriceForTakerAmount(MonetaryAmount("0.24", "ETH")), MonetaryAmount("1301 EUR"));
  EXPECT_EQ(marketOrderBook.computeAvgPriceForTakerAmount(MonetaryAmount("1000 EUR")), MonetaryAmount("1302 EUR"));
  EXPECT_EQ(marketOrderBook.computeAvgPriceForTakerAmount(MonetaryAmount("5000 EUR")),
            MonetaryAmount("1302.31760282 EUR"));
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
  MarketOrderBookTestCase2()
      : marketOrderBook(
            Market("APM", "KRW"),
            std::array<OrderBookLine, 9>{
                OrderBookLine(MonetaryAmount("1991.3922", "APM"), MonetaryAmount("57.8", "KRW"), true),
                OrderBookLine(MonetaryAmount("90184.3951", "APM"), MonetaryAmount("57.81", "KRW"), true),
                OrderBookLine(MonetaryAmount("91.1713", "APM"), MonetaryAmount("57.84", "KRW"), true),
                OrderBookLine(MonetaryAmount("41.0131", "APM"), MonetaryAmount("57.9", "KRW"), true),
                OrderBookLine(MonetaryAmount("33.5081914157147802", "APM"), MonetaryAmount("57.78", "KRW"), true),
                OrderBookLine(MonetaryAmount("3890.879", "APM"), MonetaryAmount("57.19", "KRW"), false),
                OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.18", "KRW"), false),
                OrderBookLine(MonetaryAmount("14", "APM"), MonetaryAmount("57.17", "KRW"), false),
                OrderBookLine(MonetaryAmount("3848.8453", "APM"), MonetaryAmount("57.16", "KRW"), false)}) {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  MarketOrderBook marketOrderBook;
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

class MarketOrderBookTestCase3 : public ::testing::Test {
 protected:
  MarketOrderBookTestCase3()
      : marketOrderBook(
            Market("XLM", "BTC"),
            std::array<OrderBookLine, 6>{
                OrderBookLine(MonetaryAmount("126881.164", "XLM"), MonetaryAmount("0.000007130", "BTC"), true),
                OrderBookLine(MonetaryAmount("95716.519", "XLM"), MonetaryAmount("0.000007120", "BTC"), true),
                OrderBookLine(MonetaryAmount("23726.285", "XLM"), MonetaryAmount("0.000007110", "BTC"), true),
                OrderBookLine(MonetaryAmount("37863.710", "XLM"), MonetaryAmount("0.000007100", "BTC"), false),
                OrderBookLine(MonetaryAmount("169165.594", "XLM"), MonetaryAmount("0.000007090", "BTC"), false),
                OrderBookLine(MonetaryAmount("204218.966", "XLM"), MonetaryAmount("0.000007080", "BTC"), false)}) {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  MarketOrderBook marketOrderBook;
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
  MarketOrderBookTestCaseExtended1()
      : marketOrderBook(MonetaryAmount("2300.45 EUR"), MonetaryAmount("193.09 ADA"), MonetaryAmount("2300.4 EUR"),
                        MonetaryAmount("41 ADA"), {2, 2}, 50) {}
  virtual void SetUp() {}
  virtual void TearDown() {}

  MarketOrderBook marketOrderBook;
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
  MarketOrderBook marketOrderBook(MonetaryAmount("12355.00002487 XLM"), MonetaryAmount("193.0900000000078 ADA"),
                                  MonetaryAmount("12355.00002486 XLM"), MonetaryAmount("504787104.7801 ADA"), {4, 8},
                                  10);

  EXPECT_EQ(marketOrderBook.highestBidPrice(), MonetaryAmount("12355.00002486 XLM"));
  EXPECT_EQ(marketOrderBook.lowestAskPrice(), MonetaryAmount("12355.00002487 XLM"));
}

TEST(MarketOrderBookExtendedTest, InvalidDepth) {
  EXPECT_THROW(MarketOrderBook(MonetaryAmount("1XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("5ADA"), {0, 0}, 0),
               exception);
  EXPECT_THROW(MarketOrderBook(MonetaryAmount("1XLM"), MonetaryAmount("1ADA"), MonetaryAmount("1XLM"),
                               MonetaryAmount("5ADA"), {0, 0}, -1),
               exception);
}

}  // namespace cct
