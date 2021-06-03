#include "stringoptionparser.hpp"

#include <gtest/gtest.h>

namespace cct {

TEST(StringOptionParserTest, GetExchanges) {
  EXPECT_EQ(StringOptionParser("").getExchanges(), PublicExchangeNames());
  EXPECT_EQ(StringOptionParser("kraken,upbit").getExchanges(), PublicExchangeNames({"kraken", "upbit"}));
  EXPECT_EQ(StringOptionParser("huobi_user1").getExchanges(), PublicExchangeNames({"huobi_user1"}));
}

TEST(StringOptionParserTest, GetPrivateExchanges) {
  EXPECT_EQ(StringOptionParser("").getPrivateExchanges(), PrivateExchangeNames());
  EXPECT_EQ(StringOptionParser("bithumb,binance_user1").getPrivateExchanges(),
            PrivateExchangeNames({PrivateExchangeName("bithumb"), PrivateExchangeName("binance", "user1")}));
  EXPECT_EQ(StringOptionParser("binance_user2,bithumb,binance_user1").getPrivateExchanges(),
            PrivateExchangeNames({PrivateExchangeName("binance", "user2"), PrivateExchangeName("bithumb"),
                                  PrivateExchangeName("binance", "user1")}));
}

TEST(StringOptionParserTest, GetMarketExchanges) {
  EXPECT_EQ(StringOptionParser("eth-eur").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("ETH", "EUR"), PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("dash-krw,bithumb,upbit").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("DASH", "KRW"), PublicExchangeNames({"bithumb", "upbit"})));
}

TEST(StringOptionParserTest, GetMonetaryAmountExchanges) {
  EXPECT_EQ(StringOptionParser("45.09ADA").getMonetaryAmountExchanges(),
            StringOptionParser::MonetaryAmountExchanges(MonetaryAmount("45.09ADA"), PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("-0.6509btc,kraken").getMonetaryAmountExchanges(),
            StringOptionParser::MonetaryAmountExchanges(MonetaryAmount("-0.6509BTC"), PublicExchangeNames({"kraken"})));
}

TEST(StringOptionParserTest, GetMonetaryAmountCurrencyCodePrivateExchange) {
  EXPECT_EQ(StringOptionParser("45.09ADA-eur,bithumb").getMonetaryAmountCurrencyCodePrivateExchange(),
            StringOptionParser::MonetaryAmountCurrencyCodePrivateExchange(
                MonetaryAmount("45.09ADA"), CurrencyCode("EUR"), PrivateExchangeName("bithumb")));
  EXPECT_EQ(StringOptionParser("0.02btc-xlm,upbit_user1").getMonetaryAmountCurrencyCodePrivateExchange(),
            StringOptionParser::MonetaryAmountCurrencyCodePrivateExchange(
                MonetaryAmount("0.02BTC"), CurrencyCode("XLM"), PrivateExchangeName("upbit", "user1")));
}

TEST(StringOptionParserTest, GetMonetaryAmountFromToPrivateExchange) {
  EXPECT_EQ(StringOptionParser("0.102btc,huobi-kraken").getMonetaryAmountFromToPrivateExchange(),
            StringOptionParser::MonetaryAmountFromToPrivateExchange(
                MonetaryAmount("0.102BTC"), PrivateExchangeName("huobi"), PrivateExchangeName("kraken")));
  EXPECT_EQ(
      StringOptionParser("3795541.90XLM,bithumb_user1-binance").getMonetaryAmountFromToPrivateExchange(),
      StringOptionParser::MonetaryAmountFromToPrivateExchange(
          MonetaryAmount("3795541.90XLM"), PrivateExchangeName("bithumb", "user1"), PrivateExchangeName("binance")));
  EXPECT_EQ(
      StringOptionParser("4.106eth,kraken_user2-huobi_user3").getMonetaryAmountFromToPrivateExchange(),
      StringOptionParser::MonetaryAmountFromToPrivateExchange(
          MonetaryAmount("4.106ETH"), PrivateExchangeName("kraken", "user2"), PrivateExchangeName("huobi", "user3")));
}

TEST(StringOptionParserTest, GetCurrencyCodePublicExchanges) {
  EXPECT_EQ(StringOptionParser("btc").getCurrencyCodePublicExchanges(),
            StringOptionParser::CurrencyCodePublicExchanges(CurrencyCode("BTC"), PublicExchangeNames()));
  EXPECT_EQ(
      StringOptionParser("eur,kraken_user1").getCurrencyCodePublicExchanges(),
      StringOptionParser::CurrencyCodePublicExchanges(CurrencyCode("EUR"), PublicExchangeNames({"kraken_user1"})));
  EXPECT_EQ(
      StringOptionParser("eur,binance,huobi").getCurrencyCodePublicExchanges(),
      StringOptionParser::CurrencyCodePublicExchanges(CurrencyCode("EUR"), PublicExchangeNames({"binance", "huobi"})));
}

}  // namespace cct