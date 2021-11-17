
#include "balanceportfolio.hpp"

#include <gtest/gtest.h>

namespace cct {

class BalancePortfolioTest1 : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  BalancePortfolio balancePortfolio;
};

TEST_F(BalancePortfolioTest1, Instantiate) { EXPECT_TRUE(balancePortfolio.empty()); }

TEST_F(BalancePortfolioTest1, NoEquivalentCurrency1) {
  balancePortfolio.add(MonetaryAmount("10 EUR"));

  EXPECT_FALSE(balancePortfolio.empty());
  EXPECT_EQ(balancePortfolio.get("EUR"), MonetaryAmount("10 EUR"));
  EXPECT_EQ(balancePortfolio.get("BTC"), MonetaryAmount("0 BTC"));
}

TEST_F(BalancePortfolioTest1, NoEquivalentCurrency2) {
  balancePortfolio.add(MonetaryAmount("10 EUR"));
  balancePortfolio.add(MonetaryAmount("0.45 BTC"));
  balancePortfolio.add(MonetaryAmount("11704.5678 XRP"));
  balancePortfolio.add(MonetaryAmount("215 XLM"));

  EXPECT_EQ(balancePortfolio.get("EUR"), MonetaryAmount("10 EUR"));
  EXPECT_EQ(balancePortfolio.get("XLM"), MonetaryAmount("215 XLM"));
  EXPECT_EQ(balancePortfolio.get("BTC"), MonetaryAmount("0.45 BTC"));
  EXPECT_EQ(balancePortfolio.get("ETH"), MonetaryAmount("0 ETH"));
}

class BalancePortfolioTest2 : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  BalancePortfolioTest2() {
    balancePortfolio.add(MonetaryAmount("11704.5678 XRP"));
    balancePortfolio.add(MonetaryAmount("10 EUR"));
    balancePortfolio.add(MonetaryAmount("0.45 BTC"));
    balancePortfolio.add(MonetaryAmount("215 XLM"));
  }

  BalancePortfolio balancePortfolio, o;
};

TEST_F(BalancePortfolioTest2, AddBalancePortfolio1) {
  o.add(MonetaryAmount("3.5 USD"));
  o.add(MonetaryAmount("0.45 XRP"));
  balancePortfolio += o;
  EXPECT_EQ(balancePortfolio.size(), 5U);
  EXPECT_EQ(balancePortfolio.get("XLM"), MonetaryAmount("215 XLM"));
  EXPECT_EQ(balancePortfolio.get("USD"), MonetaryAmount("3.5 USD"));
  EXPECT_EQ(balancePortfolio.get("BTC"), MonetaryAmount("0.45 BTC"));
  EXPECT_EQ(balancePortfolio.get("XRP"), MonetaryAmount("11705.0178 XRP"));
}

TEST_F(BalancePortfolioTest2, AddBalancePortfolioItself) {
  balancePortfolio += balancePortfolio;
  EXPECT_EQ(balancePortfolio.size(), 4U);
  EXPECT_EQ(balancePortfolio.get("XLM"), MonetaryAmount("430 XLM"));
  EXPECT_EQ(balancePortfolio.get("EUR"), MonetaryAmount("20 EUR"));
  EXPECT_EQ(balancePortfolio.get("BTC"), MonetaryAmount("0.9 BTC"));
  EXPECT_EQ(balancePortfolio.get("XRP"), MonetaryAmount("23409.1356 XRP"));
}

}  // namespace cct