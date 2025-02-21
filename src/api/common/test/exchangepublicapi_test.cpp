#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "default-data-dir.hpp"
#include "exchange-name-enum.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "general-config.hpp"
#include "loadconfiguration.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "monitoringinfo.hpp"
#include "reader.hpp"
#include "runmodes.hpp"
#include "timedef.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct::api {
namespace {
class StableCoinReader : public Reader {
  [[nodiscard]] string readAll() const override { return R"({"USDT": "USD"})"; }
};

class FiatConverterReader : public Reader {
  [[nodiscard]] string readAll() const override {
    return R"(
{
  "KRW-EUR": {
    "rate": 0.000697,
    "timeepoch": 1709576375
  },
  "EUR-KRW": {
    "rate": 1444.94,
    "timeepoch": 1709576451
  }
}
)";
  }
};

}  // namespace
class ExchangePublicTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{runMode,          loadConfiguration, schema::GeneralConfig(), LoggingInfo(),
                                MonitoringInfo(), Reader(),          StableCoinReader()};
  CommonAPI commonAPI{coincenterInfo, Duration::max()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max(), FiatConverterReader(), Reader()};
  MockExchangePublic exchangePublic{ExchangeNameEnum::binance, fiatConverter, commonAPI, coincenterInfo};

  MarketSet markets{{"BTC", "EUR"}, {"XLM", "EUR"},  {"ETH", "EUR"},  {"ETH", "BTC"},  {"BTC", "KRW"},
                    {"USD", "EOS"}, {"SHIB", "ICP"}, {"AVAX", "ICP"}, {"AVAX", "USDT"}};
};

namespace {
using CurrenciesPath = ExchangePublic::CurrenciesPath;
}  // namespace

TEST_F(ExchangePublicTest, FindConversionPath) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillRepeatedly(::testing::Return(markets));

  EXPECT_EQ(exchangePublic.findMarketsPath("BTC", "XLM"), MarketsPath({Market{"BTC", "EUR"}, Market{"XLM", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("XLM", "ETH"), MarketsPath({Market{"XLM", "EUR"}, Market{"ETH", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("ETH", "KRW"), MarketsPath({Market{"ETH", "BTC"}, Market{"BTC", "KRW"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("EUR", "BTC"), MarketsPath({Market{"BTC", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("SHIB", "USDT"),
            MarketsPath({Market{"SHIB", "ICP"}, Market{"AVAX", "ICP"}, Market{"AVAX", "USDT"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("SHIB", "KRW"), MarketsPath());

  EXPECT_EQ(exchangePublic.findMarketsPath("EUR", "GBP"), MarketsPath());

  EXPECT_EQ(exchangePublic.findMarketsPath("SHIB", "KRW",
                                           ExchangePublic::MarketPathMode::kWithPossibleFiatConversionAtExtremity),
            MarketsPath({Market{"SHIB", "ICP"}, Market{"AVAX", "ICP"}, Market{"AVAX", "USDT"},
                         Market{"USDT", "KRW", Market::Type::kFiatConversionMarket}}));

  EXPECT_EQ(exchangePublic.findMarketsPath("GBP", "EOS",
                                           ExchangePublic::MarketPathMode::kWithPossibleFiatConversionAtExtremity),
            MarketsPath({
                Market{"GBP", "USD", Market::Type::kFiatConversionMarket},
                Market{"USD", "EOS"},
            }));
}

TEST_F(ExchangePublicTest, FindCurrenciesPath) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillRepeatedly(::testing::Return(markets));

  EXPECT_EQ(exchangePublic.findCurrenciesPath("BTC", "XLM"), CurrenciesPath({"BTC", "EUR", "XLM"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("XLM", "ETH"), CurrenciesPath({"XLM", "EUR", "ETH"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("ETH", "KRW"), CurrenciesPath({"ETH", "BTC", "KRW"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("EUR", "BTC"), CurrenciesPath({"EUR", "BTC"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("SHIB", "KRW"), CurrenciesPath());
}

TEST_F(ExchangePublicTest, RetrieveMarket) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(::testing::Return(markets));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW").value_or(Market()), Market("BTC", "KRW"));
  EXPECT_EQ(ExchangePublic::RetrieveMarket("KRW", "BTC", markets).value_or(Market()), Market("BTC", "KRW"));
  EXPECT_FALSE(ExchangePublic::RetrieveMarket("EUR", "EOS", markets).has_value());
}

TEST_F(ExchangePublicTest, DetermineMarketFromMarketStrFilter) {
  MarketSet emptyMarkets;
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("btcusdt", emptyMarkets, "btc").value_or(Market()),
            Market("BTC", "USDT"));
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("btcusdt", emptyMarkets, "usdt").value_or(Market()),
            Market("BTC", "USDT"));
}

TEST_F(ExchangePublicTest, DetermineMarketFromMarketStrNoFilter) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(::testing::Return(markets));

  MarketSet emptyMarkets;
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("btcusdt", emptyMarkets).value_or(Market()), Market());
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("avaxicp", emptyMarkets).value_or(Market()),
            Market("AVAX", "ICP"));
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("icpavax", emptyMarkets).value_or(Market()),
            Market("AVAX", "ICP"));
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("btckrw", emptyMarkets).value_or(Market()),
            Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("krwbtc", emptyMarkets).value_or(Market()),
            Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.determineMarketFromMarketStr("ethusd", emptyMarkets).value_or(Market()), Market());
}

class ExchangePublicConvertTest : public ExchangePublicTest {
 protected:
  CurrencyCodeSet fiats{"EUR", "USD", "KRW"};

  VolAndPriNbDecimals volAndPriDec1{2, 6};
  int depth = 10;
  TimePoint time;

  MonetaryAmount askPrice1{"0.000017 BTC"};
  MonetaryAmount bidPrice1{"0.000016 BTC"};
  MarketOrderBook marketOrderBook1{
      time, askPrice1, MonetaryAmount(40000, "XLM"), bidPrice1, MonetaryAmount(25000, "XLM"), volAndPriDec1, depth};

  VolAndPriNbDecimals volAndPriDec2{2, 4};

  MonetaryAmount askPrice2{"0.0063 BTC"};
  MonetaryAmount bidPrice2{"0.0062 BTC"};
  MarketOrderBook marketOrderBook2{
      time, askPrice2, MonetaryAmount(680, "XRP"), bidPrice2, MonetaryAmount(1479, "XRP"), volAndPriDec2, depth};

  VolAndPriNbDecimals volAndPriDec3{2, 2};

  MonetaryAmount askPrice3{"37.5 EUR"};
  MonetaryAmount bidPrice3{"37.49 EUR"};
  MarketOrderBook marketOrderBook3{
      time, askPrice3, MonetaryAmount("12.04 SOL"), bidPrice3, MonetaryAmount("0.45 SOL"), volAndPriDec3, depth};

  VolAndPriNbDecimals volAndPriDec4{4, 4};

  MonetaryAmount askPrice4{"0.0021 BTC"};
  MonetaryAmount bidPrice4{"0.002 BTC"};
  MarketOrderBook marketOrderBook4{
      time, askPrice4, MonetaryAmount("5.3 SOL"), bidPrice4, MonetaryAmount("6.94 SOL"), volAndPriDec4, depth};

  MarketOrderBookMap marketOrderBookMap{{Market("XLM", "BTC"), marketOrderBook1},
                                        {Market("XRP", "BTC"), marketOrderBook2},
                                        {Market("SOL", "EUR"), marketOrderBook3},
                                        {Market("SOL", "BTC"), marketOrderBook4}};

  PriceOptions priceOptions;
};

TEST_F(ExchangePublicConvertTest, ConvertImpossible) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"BTC"};
  MarketsPath conversionPath;

  ASSERT_FALSE(exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions));
}

TEST_F(ExchangePublicConvertTest, ConvertSimple) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"BTC"};
  MarketsPath conversionPath{{"XLM", "BTC"}};

  std::optional<MonetaryAmount> ret =
      exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);
  ASSERT_TRUE(ret.has_value());
  MonetaryAmount res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook1.convert(from, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);
  EXPECT_EQ(ret, std::optional<MonetaryAmount>(res));
}

TEST_F(ExchangePublicConvertTest, ConvertDouble) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"XRP"};
  MarketsPath conversionPath{{"XLM", "BTC"}, {"XRP", "BTC"}};

  std::optional<MonetaryAmount> ret =
      exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);

  ASSERT_TRUE(ret.has_value());

  MonetaryAmount res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook1.convert(from, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);
  res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook2.convert(res, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);
  EXPECT_EQ(ret.value_or(MonetaryAmount{}), res);
}

TEST_F(ExchangePublicConvertTest, ConvertWithFiatAtBeginning) {
  MonetaryAmount from{50000, "KRW"};
  CurrencyCode toCurrency{"SOL"};
  MarketsPath conversionPath{{"KRW", "EUR", Market::Type::kFiatConversionMarket}, {"SOL", "EUR"}};

  std::optional<MonetaryAmount> ret =
      exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);

  ASSERT_TRUE(ret.has_value());

  auto optRes = fiatConverter.convert(from, "EUR");

  ASSERT_TRUE(optRes.has_value());

  MonetaryAmount res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook3.convert(optRes.value_or(MonetaryAmount{}), priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);

  EXPECT_EQ(ret.value_or(MonetaryAmount{}), res);
}

TEST_F(ExchangePublicConvertTest, ConvertWithFiatAtEnd) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"KRW"};
  MarketsPath conversionPath{
      {"XLM", "BTC"}, {"SOL", "BTC"}, {"SOL", "EUR"}, {"EUR", "KRW", Market::Type::kFiatConversionMarket}};

  std::optional<MonetaryAmount> ret =
      exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);

  ASSERT_TRUE(ret.has_value());

  MonetaryAmount res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook1.convert(from, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);
  res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook4.convert(res, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);
  res = exchangePublic.exchangeConfig().tradeFees.applyFee(
      marketOrderBook3.convert(res, priceOptions).value_or(MonetaryAmount{-1}),
      schema::ExchangeTradeFeesConfig::FeeType::Maker);

  EXPECT_EQ(ret, fiatConverter.convert(res, toCurrency));
}

}  // namespace cct::api