#include "stringoptionparser.hpp"

#include <gtest/gtest.h>

#include <tuple>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"

namespace cct {

TEST(StringOptionParserTest, GetExchanges) {
  EXPECT_TRUE(StringOptionParser("").getExchanges().empty());
  EXPECT_EQ(StringOptionParser("kraken,upbit").getExchanges(),
            ExchangeNames({ExchangeName("kraken"), ExchangeName("upbit")}));
  EXPECT_EQ(StringOptionParser("huobi_user1").getExchanges(), ExchangeNames({ExchangeName("huobi_user1")}));
}

TEST(StringOptionParserTest, GetCurrencyPrivateExchanges) {
  auto optionalCur = StringOptionParser::CurrencyIs::kOptional;
  EXPECT_EQ(StringOptionParser("").getCurrencyPrivateExchanges(optionalCur),
            std::make_pair(CurrencyCode(), ExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur").getCurrencyPrivateExchanges(optionalCur),
            std::make_pair(CurrencyCode("EUR"), ExchangeNames()));
  EXPECT_EQ(StringOptionParser("kraken1").getCurrencyPrivateExchanges(optionalCur),
            std::make_pair(CurrencyCode("kraken1"), ExchangeNames()));
  EXPECT_EQ(StringOptionParser("bithumb,binance_user1").getCurrencyPrivateExchanges(optionalCur),
            std::make_pair(CurrencyCode(), ExchangeNames({ExchangeName("bithumb"), ExchangeName("binance", "user1")})));
  EXPECT_EQ(StringOptionParser("binance_user2,bithumb,binance_user1").getCurrencyPrivateExchanges(optionalCur),
            std::make_pair(CurrencyCode(), ExchangeNames({ExchangeName("binance", "user2"), ExchangeName("bithumb"),
                                                          ExchangeName("binance", "user1")})));
  EXPECT_EQ(
      StringOptionParser("krw,Bithumb,binance_user1")
          .getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kMandatory),
      std::make_pair(CurrencyCode("KRW"), ExchangeNames({ExchangeName("bithumb"), ExchangeName("binance", "user1")})));

  EXPECT_THROW(StringOptionParser("toolongcurrency,Bithumb,binance_user1").getCurrencyPrivateExchanges(optionalCur),
               invalid_argument);
  EXPECT_THROW(StringOptionParser("binance_user1,bithumb")
                   .getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kMandatory),
               invalid_argument);
}

TEST(StringOptionParserTest, GetMarketExchanges) {
  EXPECT_EQ(StringOptionParser("eth-eur").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("ETH", "EUR"), ExchangeNames()));
  EXPECT_EQ(StringOptionParser("dash-krw,bithumb,upbit").getMarketExchanges(),
            StringOptionParser::MarketExchanges(Market("DASH", "KRW"),
                                                ExchangeNames({ExchangeName("bithumb"), ExchangeName("upbit")})));

  EXPECT_THROW(StringOptionParser("dash-toolongcurrency,bithumb,upbit").getMarketExchanges(), invalid_argument);
}

TEST(StringOptionParserTest, GetMonetaryAmountPrivateExchanges) {
  EXPECT_EQ(StringOptionParser("45.09ADA").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("45.09ADA"), false, ExchangeNames{}));
  EXPECT_EQ(StringOptionParser("15%ADA").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("15ADA"), true, ExchangeNames{}));
  EXPECT_EQ(StringOptionParser("-0.6509btc,kraken").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("-0.6509BTC"), false, ExchangeNames({ExchangeName("kraken")})));
  EXPECT_EQ(StringOptionParser("49%luna,bithumb_my_user").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount(49, "LUNA"), true, ExchangeNames({ExchangeName("bithumb", "my_user")})));
  EXPECT_EQ(StringOptionParser("10985.4006xlm,huobi,binance_user1").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(MonetaryAmount("10985.4006xlm"), false,
                            ExchangeNames({ExchangeName("huobi"), ExchangeName("binance", "user1")})));
  EXPECT_EQ(StringOptionParser("-7.009%fil,upbit,kucoin_MyUsername,binance").getMonetaryAmountPrivateExchanges(),
            std::make_tuple(
                MonetaryAmount("-7.009fil"), true,
                ExchangeNames({ExchangeName("upbit"), ExchangeName("kucoin", "MyUsername"), ExchangeName("binance")})));
}

TEST(StringOptionParserTest, GetMonetaryAmountCurrencyCodePrivateExchanges) {
  EXPECT_EQ(StringOptionParser("45.09ADA-eur,bithumb").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("45.09ADA"), false, CurrencyCode("EUR"),
                            ExchangeNames(1, ExchangeName("bithumb"))));
  EXPECT_EQ(StringOptionParser("0.02 btc-xlm,upbit_user1,binance").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("0.02BTC"), false, CurrencyCode("XLM"),
                            ExchangeNames({ExchangeName("upbit", "user1"), ExchangeName("binance")})));
  EXPECT_EQ(StringOptionParser("2500.5 eur-sol").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("2500.5 EUR"), false, CurrencyCode("SOL"), ExchangeNames()));
  EXPECT_EQ(
      StringOptionParser("17%eur-sol,kraken").getMonetaryAmountCurrencyPrivateExchanges(),
      std::make_tuple(MonetaryAmount("17EUR"), true, CurrencyCode("sol"), ExchangeNames(1, ExchangeName("kraken"))));
  EXPECT_EQ(StringOptionParser("50.035%btc-KRW,upbit,bithumb_user2").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("50.035 BTC"), true, CurrencyCode("KRW"),
                            ExchangeNames({ExchangeName("upbit"), ExchangeName("bithumb", "user2")})));
  EXPECT_EQ(StringOptionParser("-056.04%sol-jpy").getMonetaryAmountCurrencyPrivateExchanges(),
            std::make_tuple(MonetaryAmount("-56.04sol"), true, CurrencyCode("JPY"), ExchangeNames{}));
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
            std::make_pair(CurrencyCode("BTC"), ExchangeNames{ExchangeName("huobi"), ExchangeName("kraken")}));
  EXPECT_EQ(
      StringOptionParser("XLM,bithumb_user1-binance").getCurrencyFromToPrivateExchange(),
      std::make_pair(CurrencyCode("XLM"), ExchangeNames{ExchangeName("bithumb", "user1"), ExchangeName("binance")}));
  EXPECT_EQ(StringOptionParser("eth,kraken_user2-huobi_user3").getCurrencyFromToPrivateExchange(),
            std::make_pair(CurrencyCode("ETH"),
                           ExchangeNames{ExchangeName("kraken", "user2"), ExchangeName("huobi", "user3")}));
}

TEST(StringOptionParserTest, GetMonetaryAmountFromToPrivateExchange) {
  EXPECT_EQ(
      StringOptionParser("0.102btc,huobi-kraken").getMonetaryAmountFromToPrivateExchange(),
      std::make_tuple(MonetaryAmount("0.102BTC"), false, ExchangeNames{ExchangeName("huobi"), ExchangeName("kraken")}));
  EXPECT_EQ(StringOptionParser("3795541.90XLM,bithumb_user1-binance").getMonetaryAmountFromToPrivateExchange(),
            std::make_tuple(MonetaryAmount("3795541.90XLM"), false,
                            ExchangeNames{ExchangeName("bithumb", "user1"), ExchangeName("binance")}));
  EXPECT_EQ(StringOptionParser("4.106eth,kraken_user2-huobi_user3").getMonetaryAmountFromToPrivateExchange(),
            std::make_tuple(MonetaryAmount("4.106ETH"), false,
                            ExchangeNames{ExchangeName("kraken", "user2"), ExchangeName("huobi", "user3")}));

  EXPECT_THROW(StringOptionParser("test").getMonetaryAmountFromToPrivateExchange(), invalid_argument);
}

TEST(StringOptionParserTest, GetMonetaryAmountPercentageFromToPrivateExchange) {
  EXPECT_EQ(StringOptionParser("1%btc,huobi-kraken").getMonetaryAmountFromToPrivateExchange(),
            StringOptionParser::MonetaryAmountFromToPrivateExchange(
                MonetaryAmount("1BTC"), true, ExchangeNames{ExchangeName("huobi"), ExchangeName("kraken")}));
  EXPECT_EQ(
      StringOptionParser("90.05%XLM,bithumb_user1-binance").getMonetaryAmountFromToPrivateExchange(),
      StringOptionParser::MonetaryAmountFromToPrivateExchange(
          MonetaryAmount("90.05XLM"), true, ExchangeNames{ExchangeName("bithumb", "user1"), ExchangeName("binance")}));
  EXPECT_EQ(StringOptionParser("-50.758%eth,kraken_user2-huobi_user3").getMonetaryAmountFromToPrivateExchange(),
            StringOptionParser::MonetaryAmountFromToPrivateExchange(
                MonetaryAmount("-50.758ETH"), true,
                ExchangeNames{ExchangeName("kraken", "user2"), ExchangeName("huobi", "user3")}));
}

TEST(StringOptionParserTest, GetCurrencyPublicExchanges) {
  using CurrencyPublicExchanges = StringOptionParser::CurrencyPublicExchanges;
  EXPECT_EQ(StringOptionParser("btc").getCurrencyPublicExchanges(), CurrencyPublicExchanges("BTC", ExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur,kraken_user1").getCurrencyPublicExchanges(),
            CurrencyPublicExchanges("EUR", ExchangeNames({ExchangeName("kraken_user1")})));
  EXPECT_EQ(StringOptionParser("eur,binance,huobi").getCurrencyPublicExchanges(),
            CurrencyPublicExchanges("EUR", ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));
}

TEST(StringOptionParserTest, GetCurrencyCodesPublicExchanges) {
  using CurrencyCodesPublicExchanges = StringOptionParser::CurrenciesPublicExchanges;
  EXPECT_EQ(StringOptionParser("btc").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("BTC", CurrencyCode(), ExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur,kraken_user1").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("EUR", CurrencyCode(), ExchangeNames({ExchangeName("kraken_user1")})));
  EXPECT_EQ(StringOptionParser("eur,binance,huobi").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("EUR", CurrencyCode(),
                                         ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));

  EXPECT_EQ(StringOptionParser("avax-btc").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("AVAX", "BTC", ExchangeNames()));
  EXPECT_EQ(StringOptionParser("btc-eur,kraken_user1").getCurrenciesPublicExchanges(),
            CurrencyCodesPublicExchanges("BTC", "EUR", ExchangeNames({ExchangeName("kraken_user1")})));
  EXPECT_EQ(
      StringOptionParser("xlm-eur,binance,huobi").getCurrenciesPublicExchanges(),
      CurrencyCodesPublicExchanges("XLM", "EUR", ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));
}

TEST(StringOptionParserTest, GetCurrenciesPrivateExchanges) {
  using CurrenciesPrivateExchanges = StringOptionParser::CurrenciesPrivateExchanges;
  EXPECT_EQ(StringOptionParser("").getCurrenciesPrivateExchanges(false),
            CurrenciesPrivateExchanges("", "", ExchangeNames()));
  EXPECT_EQ(StringOptionParser("eur,kraken_user1").getCurrenciesPrivateExchanges(false),
            CurrenciesPrivateExchanges("EUR", CurrencyCode(), ExchangeNames({ExchangeName("kraken_user1")})));
  EXPECT_EQ(StringOptionParser("eur,binance,huobi").getCurrenciesPrivateExchanges(false),
            CurrenciesPrivateExchanges("EUR", CurrencyCode(),
                                       ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));
  EXPECT_EQ(
      StringOptionParser("kucoin-toto,binance,huobi").getCurrenciesPrivateExchanges(false),
      CurrenciesPrivateExchanges("KUCOIN", "TOTO", ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));
  EXPECT_EQ(StringOptionParser("kucoin,kraken,huobi").getCurrenciesPrivateExchanges(false),
            CurrenciesPrivateExchanges(
                CurrencyCode(), CurrencyCode(),
                ExchangeNames({ExchangeName("kucoin"), ExchangeName("kraken"), ExchangeName("huobi")})));
}

TEST(StringOptionParserTest, GetCurrenciesPrivateExchangesWithCurrencies) {
  using CurrenciesPrivateExchanges = StringOptionParser::CurrenciesPrivateExchanges;
  EXPECT_EQ(StringOptionParser("avax-btc").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges("AVAX", "BTC", ExchangeNames()));
  EXPECT_EQ(StringOptionParser("btc-eur,kraken_user1").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges("BTC", "EUR", ExchangeNames({ExchangeName("kraken_user1")})));
  EXPECT_EQ(StringOptionParser("xlm-eur,binance,huobi").getCurrenciesPrivateExchanges(),
            CurrenciesPrivateExchanges("XLM", "EUR", ExchangeNames({ExchangeName("binance"), ExchangeName("huobi")})));
}

TEST(StringOptionParserTest, CSVValues) {
  EXPECT_EQ(StringOptionParser("").getCSVValues(), vector<string>());
  EXPECT_EQ(StringOptionParser("val1,").getCSVValues(), vector<string>{{"val1"}});
  EXPECT_EQ(StringOptionParser("val1,value").getCSVValues(), vector<string>({{"val1"}, {"value"}}));
}

}  // namespace cct