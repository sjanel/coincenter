#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "currencycode.hpp"
#include "exchangeconfig.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "generalconfig.hpp"
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
}  // namespace
class ExchangePublicTest : public ::testing::Test {
 protected:
  settings::RunMode runMode = settings::RunMode::kTestKeys;
  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{runMode,          loadConfiguration, GeneralConfig(),
                                MonitoringInfo(), Reader(),          StableCoinReader()};
  CommonAPI commonAPI{coincenterInfo, Duration::max()};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  MockExchangePublic exchangePublic{kSupportedExchanges[0], fiatConverter, commonAPI, coincenterInfo};

  MarketSet markets{{"BTC", "EUR"}, {"XLM", "EUR"},  {"ETH", "EUR"},  {"ETH", "BTC"},  {"BTC", "KRW"},
                    {"USD", "EOS"}, {"SHIB", "ICP"}, {"AVAX", "ICP"}, {"AVAX", "USDT"}};
};

namespace {
using CurrenciesPath = ExchangePublic::CurrenciesPath;
using Fiats = ExchangePublic::Fiats;
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

  EXPECT_EQ(exchangePublic.findMarketsPath("SHIB", "KRW", ExchangePublic::MarketPathMode::kWithLastFiatConversion),
            MarketsPath({Market{"SHIB", "ICP"}, Market{"AVAX", "ICP"}, Market{"AVAX", "USDT"},
                         Market{"USDT", "KRW", Market::Type::kFiatConversionMarket}}));
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

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC", markets), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS", markets), exception);
}

class ExchangePublicConvertTest : public ExchangePublicTest {
 protected:
  Fiats fiats{"EUR", "USD", "KRW"};

  VolAndPriNbDecimals volAndPriDec1{2, 6};
  int depth = 10;
  TimePoint time{};

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

  MarketOrderBookMap marketOrderBookMap{{Market("XLM", "BTC"), marketOrderBook1},
                                        {Market("XRP", "BTC"), marketOrderBook2},
                                        {Market("SOL", "EUR"), marketOrderBook3}};

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
  MonetaryAmount res = exchangePublic.exchangeConfig().applyFee(
      marketOrderBook1.convert(from, priceOptions).value_or(MonetaryAmount{-1}), ExchangeConfig::FeeType::kMaker);
  EXPECT_EQ(ret, std::optional<MonetaryAmount>(res));
}

TEST_F(ExchangePublicConvertTest, ConvertDouble) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"XRP"};
  MarketsPath conversionPath{{"XLM", "BTC"}, {"XRP", "BTC"}};
  std::optional<MonetaryAmount> ret =
      exchangePublic.convert(from, toCurrency, conversionPath, fiats, marketOrderBookMap, priceOptions);
  ASSERT_TRUE(ret.has_value());
  MonetaryAmount res = exchangePublic.exchangeConfig().applyFee(
      marketOrderBook1.convert(from, priceOptions).value_or(MonetaryAmount{-1}), ExchangeConfig::FeeType::kMaker);
  res = exchangePublic.exchangeConfig().applyFee(
      marketOrderBook2.convert(res, priceOptions).value_or(MonetaryAmount{-1}), ExchangeConfig::FeeType::kMaker);
  EXPECT_EQ(ret, std::optional<MonetaryAmount>(res));
}

}  // namespace cct::api