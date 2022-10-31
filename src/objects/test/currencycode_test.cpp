
#include "currencycode.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(CurrencyCodeTest, Neutral) {
  CurrencyCode neutral;
  EXPECT_EQ("", neutral.str());
  EXPECT_EQ(0U, neutral.size());
}

TEST(CurrencyCodeTest, BracketsOperator) {
  EXPECT_EQ('G', CurrencyCode("gHYs5T")[0]);
  EXPECT_EQ('H', CurrencyCode("gHYs5T")[1]);
  EXPECT_EQ('Y', CurrencyCode("gHYs5T")[2]);
  EXPECT_EQ('S', CurrencyCode("gHYs5T")[3]);
  EXPECT_EQ('5', CurrencyCode("gHYs5T")[4]);
  EXPECT_EQ('T', CurrencyCode("gHYs5T")[5]);
}

TEST(CurrencyCodeTest, String) {
  EXPECT_EQ("", CurrencyCode("").str());
  EXPECT_EQ("1", CurrencyCode("1").str());
  EXPECT_EQ("GT", CurrencyCode("gT").str());
  EXPECT_EQ("PAR", CurrencyCode("PAR").str());
  EXPECT_EQ("LOKI", CurrencyCode("Loki").str());
  EXPECT_EQ("KOREA", CurrencyCode("KorEA").str());
  EXPECT_EQ("COUCOU", CurrencyCode("coucou").str());
  EXPECT_EQ("ANTIBES", CurrencyCode("anTibEs").str());
  EXPECT_EQ("LAVATORY", CurrencyCode("lavatoRY").str());
  EXPECT_EQ("FIVEPLUS1", CurrencyCode("FivePLus1").str());
  EXPECT_EQ("MAGIC4LIFE", CurrencyCode("Magic4Life").str());
}

TEST(CurrencyCodeTest, AppendString) {
  {
    string s("init");
    CurrencyCode("").appendStr(s);

    EXPECT_EQ("init", s);
  }
  {
    string s("init");
    CurrencyCode("a").appendStr(s);

    EXPECT_EQ("initA", s);
  }
  {
    string s("init2");
    CurrencyCode("67").appendStr(s);

    EXPECT_EQ("init267", s);
  }
  {
    string s("");
    CurrencyCode("").appendStr(s);

    EXPECT_EQ("", s);
  }
  {
    string s("");
    CurrencyCode("EUR").appendStr(s);

    EXPECT_EQ("EUR", s);
  }
}

TEST(CurrencyCodeTest, ExoticString) {
  EXPECT_EQ("G%&$-0_", CurrencyCode("g%&$-0_").str());
  EXPECT_EQ("()", CurrencyCode("()").str());
}

TEST(CurrencyCodeTest, InvalidString) {
  EXPECT_THROW(CurrencyCode("toolongcurrency"), invalid_argument);
  EXPECT_THROW(CurrencyCode("invchar~"), invalid_argument);
}

TEST(CurrencyCodeTest, Size) {
  EXPECT_EQ(0U, CurrencyCode("").size());
  EXPECT_EQ(1U, CurrencyCode("1").size());
  EXPECT_EQ(2U, CurrencyCode("gT").size());
  EXPECT_EQ(3U, CurrencyCode("PAR").size());
  EXPECT_EQ(4U, CurrencyCode("Loki").size());
  EXPECT_EQ(5U, CurrencyCode("KorEA").size());
  EXPECT_EQ(6U, CurrencyCode("coucou").size());
  EXPECT_EQ(7U, CurrencyCode("anTibEs").size());
  EXPECT_EQ(8U, CurrencyCode("lavatoRY").size());
  EXPECT_EQ(9U, CurrencyCode("FivePLus1").size());
  EXPECT_EQ(10U, CurrencyCode("Magic4Life").size());
}

TEST(CurrencyCodeTest, Code) {
  CurrencyCode eur = "EUR";
  CurrencyCode krw = "KRW";
  EXPECT_NE(eur.code(), krw.code());
  EXPECT_EQ(CurrencyCode("krw").code(), krw.code());
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
  EXPECT_EQ(CurrencyCode("etc").str(), "ETC");
}

TEST(CurrencyCodeTest, Constexpr) {
  static_assert(CurrencyCode("doge") == CurrencyCode("DOGE"));
  static_assert(CurrencyCode("XRP").code() != 0);
}

TEST(CurrencyCodeTest, Iterator) {
  EXPECT_NE(CurrencyCode("doge").begin(), CurrencyCode("DOGE").end());
  string s;
  for (char c : CurrencyCode("test")) {
    s.push_back(c);
  }
  EXPECT_EQ("TEST", s);
}

}  // namespace cct