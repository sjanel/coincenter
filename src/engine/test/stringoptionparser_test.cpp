#include "stringoptionparser.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "exchange-name-enum.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
namespace {
constexpr auto optional = StringOptionParser::FieldIs::kOptional;
constexpr auto mandatory = StringOptionParser::FieldIs::kMandatory;
}  // namespace

TEST(StringOptionParserTest, ParseExchangesDefaultSeparator) {
  EXPECT_TRUE(StringOptionParser("").parseExchanges().empty());
  EXPECT_EQ(StringOptionParser("kraken,upbit").parseExchanges(),
            ExchangeNames({ExchangeName(ExchangeNameEnum::kraken), ExchangeName(ExchangeNameEnum::upbit)}));
  EXPECT_EQ(StringOptionParser("huobi_user1").parseExchanges(), ExchangeNames({ExchangeName("huobi_user1")}));
}

TEST(StringOptionParserTest, ParseExchangesCustomSeparator) {
  EXPECT_TRUE(StringOptionParser("").parseExchanges('-').empty());
  EXPECT_EQ(StringOptionParser("kucoin-huobi_user1").parseExchanges('-'),
            ExchangeNames({ExchangeName(ExchangeNameEnum::kucoin), ExchangeName(ExchangeNameEnum::huobi, "user1")}));
  EXPECT_EQ(StringOptionParser("kraken_user2").parseExchanges('-'),
            ExchangeNames({ExchangeName(ExchangeNameEnum::kraken, "user2")}));
}

TEST(StringOptionParserTest, ParseMarketMandatory) {
  EXPECT_EQ(StringOptionParser("eth-eur").parseMarket(mandatory), Market("ETH", "EUR"));
  EXPECT_EQ(StringOptionParser("dash-krw,bithumb,upbit").parseMarket(mandatory), Market("DASH", "KRW"));

  EXPECT_THROW(StringOptionParser("dash").parseMarket(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("dash-toolongcurrency,bithumb,upbit").parseMarket(mandatory), invalid_argument);
}

TEST(StringOptionParserTest, ParseMarketOptional) {
  EXPECT_EQ(StringOptionParser("").parseMarket(optional), Market());
  EXPECT_EQ(StringOptionParser("eth").parseMarket(optional), Market());
  EXPECT_EQ(StringOptionParser("eth,kucoin").parseMarket(optional), Market());
  EXPECT_EQ(StringOptionParser("eth-eur").parseMarket(optional), Market("ETH", "EUR"));
  EXPECT_EQ(StringOptionParser("BTC-USDT,bithumb,upbit").parseMarket(optional), Market("BTC", "USDT"));
  EXPECT_EQ(StringOptionParser("kraken,upbit").parseMarket(optional), Market());
  EXPECT_EQ(StringOptionParser("dash-toolongcurrency,bithumb,upbit").parseMarket(optional), Market());
}

TEST(StringOptionParserTest, ParseCurrencyMandatory) {
  EXPECT_EQ(StringOptionParser("krw,kucoin,binance_user1").parseCurrency(mandatory), CurrencyCode("KRW"));

  EXPECT_THROW(StringOptionParser("").parseCurrency(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("binance_user1,bithumb").parseCurrency(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("toolongcurrency").parseCurrency(mandatory), invalid_argument);
}

TEST(StringOptionParserTest, ParseCurrencyOptional) {
  EXPECT_EQ(StringOptionParser("").parseCurrency(optional), CurrencyCode());
  EXPECT_EQ(StringOptionParser("eur").parseCurrency(optional), CurrencyCode("EUR"));
  EXPECT_EQ(StringOptionParser("kraken1").parseCurrency(optional), CurrencyCode("kraken1"));
  EXPECT_EQ(StringOptionParser("bithumb,binance_user1").parseCurrency(optional), CurrencyCode());
  EXPECT_EQ(StringOptionParser("binance_user2,bithumb,binance_user1").parseCurrency(optional), CurrencyCode());
  EXPECT_EQ(StringOptionParser("toolongcurrency,Bithumb,binance_user1").parseCurrency(optional), CurrencyCode());
}

TEST(StringOptionParserTest, ParseAmountMandatoryAbsolute) {
  EXPECT_EQ(StringOptionParser("45.09ADA").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("45.09ADA"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(StringOptionParser("0.6509btc,kraken").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("0.6509BTC"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(StringOptionParser("10985.4006xlm,huobi,binance_user1").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("10985.4006xlm"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(StringOptionParser("-0.6509btc,kraken").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("-0.6509btc"), StringOptionParser::AmountType::kAbsolute));

  EXPECT_THROW(StringOptionParser("").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("0BTC").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("eur").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("kraken").parseNonZeroAmount(mandatory), invalid_argument);
}

TEST(StringOptionParserTest, ParseAmountMandatoryPercentage) {
  EXPECT_EQ(StringOptionParser("15%ADA").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("15ADA"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(StringOptionParser("49%luna,bithumb_my_user").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount(49, "LUNA"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(StringOptionParser("7.009%fil,upbit,kucoin_MyUsername,binance").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("7.009fil"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(StringOptionParser("-0.009%fil,upbit,kucoin_MyUsername,binance").parseNonZeroAmount(mandatory),
            std::make_pair(MonetaryAmount("-0.009fil"), StringOptionParser::AmountType::kPercentage));

  EXPECT_THROW(StringOptionParser("").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("0%USDT").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("btc").parseNonZeroAmount(mandatory), invalid_argument);
  EXPECT_THROW(StringOptionParser("230.009%fil,upbit,kucoin_MyUsername,binance").parseNonZeroAmount(mandatory),
               invalid_argument);  // > 100 %
}

TEST(StringOptionParserTest, ParseAmountOptionalAbsolute) {
  EXPECT_EQ(StringOptionParser("").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(StringOptionParser("XRP").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(StringOptionParser("15ADA").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount("15ADA"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(StringOptionParser("bithumb_my_user").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(StringOptionParser("7.009fil,upbit,kucoin_MyUsername,binance").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount("7.009fil"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(StringOptionParser("-7.009shib,upbit,kucoin_MyUsername,binance").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount("-7.009shib"), StringOptionParser::AmountType::kAbsolute));
}

TEST(StringOptionParserTest, ParseAmountOptionalPercentage) {
  EXPECT_EQ(StringOptionParser("0%ADA").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(StringOptionParser("45.09%ADA").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount("45.09ADA"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(StringOptionParser("0.6509%btc,kraken").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount("0.6509BTC"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(StringOptionParser("huobi,binance_user1").parseNonZeroAmount(optional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(StringOptionParser("-78%btc,kraken").parseNonZeroAmount(),
            std::make_pair(MonetaryAmount(-78, "BTC"), StringOptionParser::AmountType::kPercentage));
}

TEST(StringOptionParserTest, CSVValues) {
  EXPECT_EQ(StringOptionParser("").getCSVValues(), vector<string>());
  EXPECT_EQ(StringOptionParser("val1,").getCSVValues(), vector<string>{{"val1"}});
  EXPECT_EQ(StringOptionParser("val1,value").getCSVValues(), vector<string>({{"val1"}, {"value"}}));
}

TEST(StringOptionParserTest, AmountExchangesFlow) {
  StringOptionParser parser("34.8XRP,kraken,huobi_long_user1");

  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount("34.8XRP"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));

  EXPECT_EQ(parser.parseExchanges(','), ExchangeNames({ExchangeName(ExchangeNameEnum::kraken),
                                                       ExchangeName(ExchangeNameEnum::huobi, "long_user1")}));

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, AmountCurrencyNoExchangesFlow) {
  StringOptionParser parser("0.56%BTC-krw");

  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount("0.56BTC"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kMandatory), CurrencyCode("KRW"));

  EXPECT_EQ(parser.parseExchanges('-'), ExchangeNames());

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, AmountCurrencyWithExchangesFlow) {
  StringOptionParser parser("15.9DOGE-USDT,binance_long_user2,kucoin");

  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount("15.9DOGE"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kMandatory), CurrencyCode("USDT"));

  EXPECT_EQ(parser.parseExchanges(','), ExchangeNames({ExchangeName(ExchangeNameEnum::binance, "long_user2"),
                                                       ExchangeName(ExchangeNameEnum::kucoin)}));

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, SeveralAmountCurrencyExchangesFlow) {
  StringOptionParser parser("98.05%JST--67.4BTC-hydrA,binance-kraken");

  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kMandatory),
            std::make_pair(MonetaryAmount("98.05JST"), StringOptionParser::AmountType::kPercentage));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount("-67.4BTC"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode("HYDRA"));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());

  EXPECT_EQ(parser.parseExchanges('-'),
            ExchangeNames({ExchangeName(ExchangeNameEnum::binance), ExchangeName(ExchangeNameEnum::kraken)}));

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, ExchangesNotLast) {
  StringOptionParser parser("jst,34.78966544ETH,kucoin_user1-binance-kraken,krw");

  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode("JST"));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kMandatory),
            std::make_pair(MonetaryAmount("34.78966544ETH"), StringOptionParser::AmountType::kAbsolute));
  EXPECT_EQ(parser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional),
            std::make_pair(MonetaryAmount(), StringOptionParser::AmountType::kNotPresent));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kOptional), CurrencyCode());

  EXPECT_EQ(parser.parseExchanges('-', ','),
            ExchangeNames({ExchangeName(ExchangeNameEnum::kucoin, "user1"), ExchangeName(ExchangeNameEnum::binance),
                           ExchangeName(ExchangeNameEnum::kraken)}));
  EXPECT_EQ(parser.parseCurrency(StringOptionParser::FieldIs::kMandatory), CurrencyCode("KRW"));

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, ParseDurationMandatory) {
  StringOptionParser parser(" 45min83s,kraken,upbit");

  EXPECT_EQ(parser.parseDuration(StringOptionParser::FieldIs::kMandatory),
            std::chrono::minutes{45} + std::chrono::seconds{83});
  EXPECT_EQ(parser.parseExchanges(',', '\0'),
            ExchangeNames({ExchangeName(ExchangeNameEnum::kraken), ExchangeName(ExchangeNameEnum::upbit)}));

  EXPECT_NO_THROW(parser.checkEndParsing());
}

TEST(StringOptionParserTest, ParseDurationOptional) {
  StringOptionParser parser("binance,huobi_user1,34h 4500ms");

  EXPECT_EQ(parser.parseDuration(StringOptionParser::FieldIs::kOptional), kUndefinedDuration);
  EXPECT_EQ(parser.parseExchanges(',', '\0'),
            ExchangeNames({ExchangeName(ExchangeNameEnum::binance), ExchangeName(ExchangeNameEnum::huobi, "user1")}));

  EXPECT_EQ(parser.parseDuration(StringOptionParser::FieldIs::kOptional),
            std::chrono::hours{34} + std::chrono::milliseconds{4500});

  EXPECT_NO_THROW(parser.checkEndParsing());
}

}  // namespace cct