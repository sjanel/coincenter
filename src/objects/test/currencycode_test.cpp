
#include "currencycode.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CurrencyCodeTest, Instantiate) {
  CurrencyCode eur = "EUR";
  EXPECT_EQ("EUR", eur.str());
  CurrencyCode krw = CurrencyCode(std::string_view("KRW"));
  EXPECT_EQ("KRW", krw.str());
}

TEST(CurrencyCodeTest, Code) {
  CurrencyCode eur = "EUR";
  CurrencyCode krw = "KRW";
  EXPECT_NE(eur.code(), krw.code());
  EXPECT_EQ(eur.code(), CurrencyCode("EUR").code());
}

TEST(CurrencyCodeTest, Comparison) {
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
}

TEST(CurrencyCodeTest, UpperConversion) {
  EXPECT_EQ(CurrencyCode("doge"), CurrencyCode("DOGE"));
  EXPECT_EQ(CurrencyCode("BtC"), CurrencyCode("BTC"));
  EXPECT_EQ(CurrencyCode("duRfVgh"), CurrencyCode("dUrfVGH"));
}

}  // namespace cct