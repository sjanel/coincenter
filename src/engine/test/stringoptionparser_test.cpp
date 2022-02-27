#include "stringoptionparser.hpp"

#include <gtest/gtest.h>

#include "cct_invalid_argument_exception.hpp"

namespace cct {

TEST(StringOptionParserTest, GetExchanges) {
  EXPECT_TRUE(StringOptionParser("").getExchanges().empty());
  EXPECT_EQ(StringOptionParser("kraken,upbit").getExchanges(), PublicExchangeNames({"kraken", "upbit"}));
  EXPECT_EQ(StringOptionParser("huobi_user1").getExchanges(), PublicExchangeNames({"huobi_user1"}));
}

TEST(StringOptionParserTest, GetCurrencyPrivateExchanges) {
  EXPECT_EQ(StringOptionParser("").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode(), PrivateExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode("EUR"), PrivateExchangeNames()));
  EXPECT_EQ(StringOptionParser("kraken1").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode("kraken1"), PrivateExchangeNames()));
  EXPECT_EQ(StringOptionParser("bithumb,binance_user1").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode(), PrivateExchangeNames({PrivateExchangeName("bithumb"),
                                                                 PrivateExchangeName("binance", "user1")})));
  EXPECT_EQ(StringOptionParser("binance_user2,bithumb,binance_user1").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode(), PrivateExchangeNames({PrivateExchangeName("binance", "user2"),
                                                                 PrivateExchangeName("bithumb"),
                                                                 PrivateExchangeName("binance", "user1")})));
  EXPECT_EQ(StringOptionParser("krw,Bithumb,binance_user1").getCurrencyPrivateExchanges(),
            std::make_pair(CurrencyCode("KRW"), PrivateExchangeNames({PrivateExchangeName("bithumb"),
                                                                      PrivateExchangeName("binance", "user1")})));
}

TEST(StringOptionParserTest, GetMarketExchanges) {
  EXPECT_EQ(StringOptionParser("eth-eur").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("ETH", "EUR"), PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("dash-krw,bithumb,upbit").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("DASH", "KRW"), PublicExchangeNames({"bithumb", "upbit"})));
}

TEST(StringOptionParserTest, GetMonetaryAmountPrivateExchanges) {
  EXPECT_EQ(StringOptionParser("45.09ADA").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("45.09ADA"), false, PrivateExchangeNames{}));
  EXPECT_EQ(StringOptionParser("15%ADA").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("15ADA"), true, PrivateExchangeNames{}));
  EXPECT_EQ(
      StringOptionParser("-0.6509btc,kraken").getMonetaryAmountPrivateExchanges(),
      std::make_tuple(MonetaryAmount("-0.6509BTC"), false, PrivateExchangeNames({PrivateExchangeName("kraken")})));
  EXPECT_EQ(StringOptionParser("49%luna,bithumb_my_user").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount(49, "LUNA"), true,
                            PrivateExchangeNames({PrivateExchangeName("bithumb", "my_user")})));
  EXPECT_EQ(
      StringOptionParser("10985.4006xlm,huobi,binance_user1").getMonetaryAmountPrivateExchanges(),
      std::make_tuple(MonetaryAmount("10985.4006xlm"), false,
                      PrivateExchangeNames({PrivateExchangeName("huobi"), PrivateExchangeName("binance", "user1")})));
  EXPECT_EQ(
      StringOptionParser("-7.009%fil,upbit,kucoin_MyUsername,binance").getMonetaryAmountPrivateExchanges(),
      std::make_tuple(MonetaryAmount("-7.009fil"), true,
                      PrivateExchangeNames({PrivateExchangeName("upbit"), PrivateExchangeName("kucoin", "MyUsername"),
                                            PrivateExchangeName("binance")})));
}

TEST(StringOptionParserTest, GetMonetaryAmountCurrencyCodePrivateExchanges) {
  EXPECT_EQ(StringOptionParser("45.09ADA-eur,bithumb").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("45.09ADA"), false, CurrencyCode("EUR"),
                            PrivateExchangeNames(1, PrivateExchangeName("bithumb"))));
  EXPECT_EQ(
      StringOptionParser("0.02 btc-xlm,upbit_user1,binance").getMonetaryAmountCurrencyPrivateExchanges(),
      std::make_tuple(MonetaryAmount("0.02BTC"), false, CurrencyCode("XLM"),
                      PrivateExchangeNames({PrivateExchangeName("upbit", "user1"), PrivateExchangeName("binance")})));
  EXPECT_EQ(StringOptionParser("2500.5 eur-sol").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("2500.5 EUR"), false, CurrencyCode("SOL"), PrivateExchangeNames()));
  EXPECT_EQ(StringOptionParser("17%eur-sol,kraken").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("17EUR"), true, CurrencyCode("sol"),
                            PrivateExchangeNames(1, PrivateExchangeName("kraken"))));
  EXPECT_EQ(
      StringOptionParser("50.035%btc-KRW,upbit,bithumb_user2").getMonetaryAmountCurrencyPrivateExchanges(),
      std::make_tuple(MonetaryAmount("50.035 BTC"), true, CurrencyCode("KRW"),
                      PrivateExchangeNames({PrivateExchangeName("upbit"), PrivateExchangeName("bithumb", "user2")})));
  EXPECT_EQ(StringOptionParser("-056.04%sol-jpy").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("-56.04sol"), true, CurrencyCode("JPY"), PrivateExchangeNames{}));
}

TEST(StringOptionParserTest, GetMonetaryAmountCurrencyCodePrivateExchangesValidity) {
  EXPECT_NO_THROW(StringOptionParser("100 % eur-sol").getMonetaryAmountCurrencyPrivateExchanges());
  EXPECT_NO_THROW(StringOptionParser("-15.709%eur-sol").getMonetaryAmountCurrencyPrivateExchanges());
  EXPECT_THROW(StringOptionParser("").getMonetaryAmountCurrencyPrivateExchanges(), invalid_argument);
  EXPECT_THROW(StringOptionParser("100.2% eur-sol").getMonetaryAmountCurrencyPrivateExchanges(), invalid_argument);
  EXPECT_THROW(StringOptionParser("-150 %eur-sol").getMonetaryAmountCurrencyPrivateExchanges(), invalid_argument);
}

TEST(StringOptionParserTest, GetCurrencyFromToPrivateExchange) {
  EXPECT_EQ(StringOptionParser("btc,huobi-kraken").getCurrencyFromToPrivateExchange(),
            std::make_tuple(CurrencyCode("BTC"), PrivateExchangeName("huobi"), PrivateExchangeName("kraken")));
  EXPECT_EQ(
      StringOptionParser("XLM,bithumb_user1-binance").getCurrencyFromToPrivateExchange(),
      std::make_tuple(CurrencyCode("XLM"), PrivateExchangeName("bithumb", "user1"), PrivateExchangeName("binance")));
  EXPECT_EQ(StringOptionParser("eth,kraken_user2-huobi_user3").getCurrencyFromToPrivateExchange(),
            std::make_tuple(CurrencyCode("ETH"), PrivateExchangeName("kraken", "user2"),
                            PrivateExchangeName("huobi", "user3")));
}

TEST(StringOptionParserTest, GetMonetaryAmountFromToPrivateExchange) {
  EXPECT_EQ(
      StringOptionParser("0.102btc,huobi-kraken").getMonetaryAmountFromToPrivateExchange(),
      std::make_tuple(MonetaryAmount("0.102BTC"), false, PrivateExchangeName("huobi"), PrivateExchangeName("kraken")));
  EXPECT_EQ(StringOptionParser("3795541.90XLM,bithumb_user1-binance").getMonetaryAmountFromToPrivateExchange(),
            std::make_tuple(MonetaryAmount("3795541.90XLM"), false, PrivateExchangeName("bithumb", "user1"),
                            PrivateExchangeName("binance")));
  EXPECT_EQ(StringOptionParser("4.106eth,kraken_user2-huobi_user3").getMonetaryAmountFromToPrivateExchange(),
            std::make_tuple(MonetaryAmount("4.106ETH"), false, PrivateExchangeName("kraken", "user2"),
                            PrivateExchangeName("huobi", "user3")));
}

TEST(StringOptionParserTest, GetMonetaryAmountPercentageFromToPrivateExchange) {
  EXPECT_EQ(StringOptionParser("1%btc,huobi-kraken").getMonetaryAmountFromToPrivateExchange(),
            StringOptionParser::MonetaryAmountFromToPrivateExchange(
                MonetaryAmount("1BTC"), true, PrivateExchangeName("huobi"), PrivateExchangeName("kraken")));
  EXPECT_EQ(
      StringOptionParser("90.05%XLM,bithumb_user1-binance").getMonetaryAmountFromToPrivateExchange(),
      StringOptionParser::MonetaryAmountFromToPrivateExchange(
          MonetaryAmount("90.05XLM"), true, PrivateExchangeName("bithumb", "user1"), PrivateExchangeName("binance")));
  EXPECT_EQ(StringOptionParser("-50.758%eth,kraken_user2-huobi_user3").getMonetaryAmountFromToPrivateExchange(),
            StringOptionParser::MonetaryAmountFromToPrivateExchange(MonetaryAmount("-50.758ETH"), true,
                                                                    PrivateExchangeName("kraken", "user2"),
                                                                    PrivateExchangeName("huobi", "user3")));
}

TEST(StringOptionParserTest, GetCurrencyPublicExchanges) {
  using CurrencyPublicExchanges = StringOptionParser::CurrencyPublicExchanges;
  EXPECT_EQ(StringOptionParser("btc").getCurrencyPublicExchanges(),
            CurrencyPublicExchanges("BTC", PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur,kraken_user1").getCurrencyPublicExchanges(),
            CurrencyPublicExchanges("EUR", PublicExchangeNames({"kraken_user1"})));
  EXPECT_EQ(StringOptionParser("eur,binance,huobi").getCurrencyPublicExchanges(),
            CurrencyPublicExchanges("EUR", PublicExchangeNames({"binance", "huobi"})));
}

TEST(StringOptionParserTest, GetCurrencyCodesPublicExchanges) {
  using CurrencyCodesPublicExchanges = StringOptionParser::CurrenciesPublicExchanges;
  EXPECT_EQ(StringOptionParser("btc").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("BTC", CurrencyCode(), PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur,kraken_user1").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("EUR", CurrencyCode(), PublicExchangeNames({"kraken_user1"})));
  EXPECT_EQ(StringOptionParser("eur,binance,huobi").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("EUR", CurrencyCode(), PublicExchangeNames({"binance", "huobi"})));

  EXPECT_EQ(StringOptionParser("avax-btc").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("AVAX", "BTC", PublicExchangeNames()));
  EXPECT_EQ(StringOptionParser("btc-eur,kraken_user1").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("BTC", "EUR", PublicExchangeNames({"kraken_user1"})));
  EXPECT_EQ(StringOptionParser("xlm-eur,binance,huobi").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("XLM", "EUR", PublicExchangeNames({"binance", "huobi"})));
}

TEST(StringOptionParserTest, GetCurrenciesPrivateExchanges) {
  using CurrenciesPrivateExchanges = StringOptionParser::CurrenciesPrivateExchanges;
  EXPECT_EQ(StringOptionParser("").getCurrenciesPrivateExchanges(false),
            CurrenciesPrivateExchanges("", "", PrivateExchangeNames()));
  EXPECT_EQ(
      StringOptionParser("eur,kraken_user1").getCurrenciesPrivateExchanges(false),
      CurrenciesPrivateExchanges("EUR", CurrencyCode(), PrivateExchangeNames({PrivateExchangeName("kraken_user1")})));
  EXPECT_EQ(
      StringOptionParser("eur,binance,huobi").getCurrenciesPrivateExchanges(false),
      CurrenciesPrivateExchanges("EUR", CurrencyCode(),
                                 PrivateExchangeNames({PrivateExchangeName("binance"), PrivateExchangeName("huobi")})));

  EXPECT_EQ(StringOptionParser("avax-btc").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges("AVAX", "BTC", PrivateExchangeNames()));
  EXPECT_EQ(StringOptionParser("btc-eur,kraken_user1").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges("BTC", "EUR", PrivateExchangeNames({PrivateExchangeName("kraken_user1")})));
  EXPECT_EQ(StringOptionParser("xlm-eur,binance,huobi").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges(
                "XLM", "EUR", PrivateExchangeNames({PrivateExchangeName("binance"), PrivateExchangeName("huobi")})));
}

TEST(StringOptionParserTest, CSVValues) {
  EXPECT_EQ(StringOptionParser("").getCSVValues(), vector<std::string_view>());
  EXPECT_EQ(StringOptionParser("val1,").getCSVValues(), vector<std::string_view>{{"val1"}});
  EXPECT_EQ(StringOptionParser("val1,value").getCSVValues(), vector<std::string_view>({{"val1"}, {"value"}}));
}

}  // namespace cct