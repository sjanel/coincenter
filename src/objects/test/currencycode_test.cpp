
#include "currencycode.hpp"

#include <gtest/gtest.h>

#include <algorithm>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"

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

TEST(CurrencyCodeTest, IsValid) {
  EXPECT_TRUE(CurrencyCode::IsValid(""));
  EXPECT_TRUE(CurrencyCode::IsValid("BTC"));
  EXPECT_TRUE(CurrencyCode::IsValid("TESTCUR"));
  EXPECT_TRUE(CurrencyCode::IsValid("lowCase"));

  EXPECT_FALSE(CurrencyCode::IsValid("averylongcurrency"));
  EXPECT_FALSE(CurrencyCode::IsValid("inv "));
}

TEST(CurrencyCodeTest, AppendString) {
  {
    string str("");
    CurrencyCode("").appendStrTo(str);

    EXPECT_EQ("", str);
  }
  {
    string str("init");
    CurrencyCode("").appendStrTo(str);

    EXPECT_EQ("init", str);
  }
  {
    string str("init");
    CurrencyCode("a").appendStrTo(str);

    EXPECT_EQ("initA", str);
  }
  {
    string str("init2");
    CurrencyCode("67").appendStrTo(str);

    EXPECT_EQ("init267", str);
  }
  {
    string str("");
    CurrencyCode("EUR").appendStrTo(str);

    EXPECT_EQ("EUR", str);
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

TEST(CurrencyCodeTest, IEqual) {
  static_assert(CurrencyCode("XRP").iequal("xrP"));
  EXPECT_TRUE(CurrencyCode("eur").iequal("EUR"));
  EXPECT_TRUE(CurrencyCode("eur").iequal("Eur"));
  EXPECT_TRUE(CurrencyCode("BABYDOGE").iequal("babyDoge"));
  EXPECT_TRUE(CurrencyCode("1INCH").iequal("1INCH"));

  static_assert(!CurrencyCode("XRP").iequal("XRG"));
  EXPECT_FALSE(CurrencyCode("eur").iequal("FUR"));
  EXPECT_FALSE(CurrencyCode("eur").iequal("EUH"));
  EXPECT_FALSE(CurrencyCode("BABYDOGE").iequal("babyoge"));
  EXPECT_FALSE(CurrencyCode("BABYDOGE").iequal("babyDog"));
  EXPECT_FALSE(CurrencyCode("inch").iequal("1INCH"));
  EXPECT_FALSE(CurrencyCode("1inc").iequal("1INCH"));
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

namespace {
constexpr bool HasZ(CurrencyCode cur) {
  return std::any_of(cur.begin(), cur.end(), [](char ch) { return ch == 'Z'; });
}
}  // namespace

TEST(CurrencyCodeTest, Constexpr) {
  static_assert(CurrencyCode("doge") == CurrencyCode("DOGE"));
  static_assert(CurrencyCode("XRP").code() != 0);

  static_assert(!HasZ(CurrencyCode("LONGCUR")));
  static_assert(HasZ(CurrencyCode("GTZFD")));

  static_assert(CurrencyCode::IsValid("btC"));
  static_assert(!CurrencyCode::IsValid("muchtoolongcur"));
}

TEST(CurrencyCodeTest, Iterator) {
  EXPECT_NE(CurrencyCode("doge").begin(), CurrencyCode("DOGE").end());
  string str;
  for (char ch : CurrencyCode("test")) {
    str.push_back(ch);
  }
  EXPECT_EQ("TEST", str);
}

}  // namespace cct