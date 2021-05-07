
#include "balanceportfolio.hpp"

#include <gtest/gtest.h>

namespace cct {

class BalancePortfolioTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  BalancePortfolio balancePortfolio;
};

TEST_F(BalancePortfolioTest, Instantiate) { EXPECT_TRUE(balancePortfolio.empty()); }

TEST_F(BalancePortfolioTest, NoEquivalentCurrency1) {
  balancePortfolio.add(MonetaryAmount("10 EUR"));

  EXPECT_FALSE(balancePortfolio.empty());
  EXPECT_EQ(balancePortfolio.getBalance("EUR"), MonetaryAmount("10 EUR"));
  EXPECT_EQ(balancePortfolio.getBalance("BTC"), MonetaryAmount("0 BTC"));
}

TEST_F(BalancePortfolioTest, NoEquivalentCurrency2) {
  balancePortfolio.add(MonetaryAmount("10 EUR"));
  balancePortfolio.add(MonetaryAmount("0.45 BTC"));
  balancePortfolio.add(MonetaryAmount("11704.5678 XRP"));
  balancePortfolio.add(MonetaryAmount("215 XLM"));

  EXPECT_EQ(balancePortfolio.getBalance("EUR"), MonetaryAmount("10 EUR"));
  EXPECT_EQ(balancePortfolio.getBalance("XLM"), MonetaryAmount("215 XLM"));
  EXPECT_EQ(balancePortfolio.getBalance("BTC"), MonetaryAmount("0.45 BTC"));
  EXPECT_EQ(balancePortfolio.getBalance("ETH"), MonetaryAmount("0 ETH"));
}

}  // namespace cct