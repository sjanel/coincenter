
#include "currencycode.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CurrencyCodeTest, Neutral) {
  CurrencyCode neutral;
  EXPECT_EQ("", neutral.str());
  EXPECT_EQ(0U, neutral.size());
}

TEST(CurrencyCodeTest, Instantiate) {
  CurrencyCode eur = "EUR";
  EXPECT_EQ("EUR", eur.str());
  EXPECT_EQ(3U, eur.size());
  EXPECT_EQ("KRW", CurrencyCode(std::string_view("KRW")).str());
}

TEST(CurrencyCodeTest, Code) {
  CurrencyCode eur = "EUR";
  CurrencyCode krw = "KRW";
  EXPECT_NE(eur.code(), krw.code());
  EXPECT_EQ(eur.code(), CurrencyCode("EUR").code());
}

TEST(CurrencyCodeTest, Equality) {
  CurrencyCode doge = "DOGE";
  CurrencyCode sushi = "SUSHI";
  CurrencyCode renbtc = "RENBTC";
  CurrencyCode doge2 = "DOGE";
  CurrencyCode sushi2(sushi.str());
  EXPECT_EQ(doge, doge2);
  EXPECT_NE(doge, renbtc);
  EXPECT_NE(sushi, doge2);
  EXPECT_EQ(sushi, sushi);
  EXPECT_EQ(sushi, sushi2);
  EXPECT_EQ(sushi2, sushi);
  EXPECT_NE(renbtc, doge2);
  EXPECT_EQ(CurrencyCode("sol"), CurrencyCode("SOL"));
  EXPECT_EQ(CurrencyCode("sol").code(), CurrencyCode("SOL").code());
}

TEST(CurrencyCodeTest, Comparison) {
  CurrencyCode doge = "DOGE";
  CurrencyCode sushi = "SUSHI";
  CurrencyCode renbtc = "RENBTC";
  CurrencyCode doge2 = "DOGE";
  CurrencyCode sushi2(sushi.str());
  EXPECT_LT(doge, renbtc);
  EXPECT_GT(sushi, renbtc);
  EXPECT_LE(sushi, sushi2);
  EXPECT_LE(doge2, sushi2);
  EXPECT_GE(renbtc, renbtc);
  EXPECT_GE(renbtc, doge);
}

TEST(CurrencyCodeTest, UpperConversion) {
  EXPECT_EQ(CurrencyCode("doge"), CurrencyCode("DOGE"));
  EXPECT_EQ(CurrencyCode("BtC"), CurrencyCode("BTC"));
  EXPECT_EQ(CurrencyCode("duRfVgh"), CurrencyCode("dUrfVGH"));
}

TEST(CurrencyCodeTest, Constexpr) {
  static_assert(CurrencyCode("doge") == CurrencyCode("DOGE"));
  static_assert(CurrencyCode("etc").str() == "ETC");
  static_assert(CurrencyCode("XRP").code() != 0);
}

}  // namespace cct