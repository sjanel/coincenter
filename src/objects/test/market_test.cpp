#include "market.hpp"

#include <gtest/gtest.h>

namespace cct {
TEST(MarketTest, DefaultConstructor) {
  Market market;

  EXPECT_TRUE(market.base().isNeutral());
  EXPECT_TRUE(market.quote().isNeutral());
  EXPECT_TRUE(market.isNeutral());
  EXPECT_FALSE(market.isDefined());
  EXPECT_EQ(Market(), market);
}

TEST(MarketTest, CurrencyConstructor) {
  Market market(CurrencyCode("ETH"), "USDT");

  EXPECT_EQ(market.base(), CurrencyCode("ETH"));
  EXPECT_EQ(market.quote(), CurrencyCode("USDT"));
  EXPECT_FALSE(market.isNeutral());
  EXPECT_TRUE(market.isDefined());
  EXPECT_EQ(Market("eth", "usdt"), market);
}

TEST(MarketTest, StringConstructor) {
  Market market("sol-KRW");

  EXPECT_EQ(market.base(), CurrencyCode("SOL"));
  EXPECT_EQ(market.quote(), CurrencyCode("KRW"));
  EXPECT_EQ(Market("sol", "KRW"), market);
}

TEST(MarketTest, IncorrectStringConstructor) {
  EXPECT_THROW(Market("sol"), exception);
  EXPECT_THROW(Market("BTC-EUR-"), exception);
}

TEST(MarketTest, StringRepresentationRegularMarket) {
  Market market("shib", "btc");

  EXPECT_EQ(market.str(), "SHIB-BTC");
  EXPECT_EQ(market.assetsPairStrUpper('/'), "SHIB/BTC");
  EXPECT_EQ(market.assetsPairStrLower('|'), "shib|btc");
}

TEST(MarketTest, StringRepresentationFiatConversionMarket) {
  Market market("USDT", "EUR", Market::Type::kFiatConversionMarket);

  EXPECT_EQ(market.str(), "*USDT-EUR");
  EXPECT_EQ(market.assetsPairStrUpper('('), "*USDT(EUR");
  EXPECT_EQ(market.assetsPairStrLower(')'), "*usdt)eur");
}
}  // namespace cct