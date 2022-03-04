#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cryptowatchapi.hpp"
#include "exchangepublicapi_mock.hpp"
#include "fiatconverter.hpp"

namespace cct::api {
class ExchangePublicTest : public ::testing::Test {
 protected:
  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CryptowatchAPI cryptowatchAPI{coincenterInfo};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  MockExchangePublic exchangePublic{kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo};
};

namespace {
using MarketsPath = ExchangePublic::MarketsPath;
using CurrenciesPath = ExchangePublic::CurrenciesPath;
using MarketSet = ExchangePublic::MarketSet;
using Fiats = ExchangePublic::Fiats;
using MarketOrderBookMap = ExchangePublic::MarketOrderBookMap;
}  // namespace

TEST_F(ExchangePublicTest, FindFastestConversionPath) {
  MarketSet markets{{"BTC", "EUR"}, {"XLM", "EUR"}, {"ETH", "EUR"}, {"ETH", "BTC"}};
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillRepeatedly(::testing::Return(markets));
  EXPECT_EQ(exchangePublic.findMarketsPath("BTC", "XLM"), MarketsPath({Market{"BTC", "EUR"}, Market{"XLM", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("XLM", "ETH"), MarketsPath({Market{"XLM", "EUR"}, Market{"ETH", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("ETH", "KRW"), MarketsPath({Market{"ETH", "EUR"}, Market{"EUR", "KRW"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("EUR", "BTC"), MarketsPath({Market{"BTC", "EUR"}}));
  EXPECT_EQ(exchangePublic.findMarketsPath("EOS", "KRW"), MarketsPath());

  EXPECT_EQ(exchangePublic.findCurrenciesPath("BTC", "XLM"), CurrenciesPath({"BTC", "EUR", "XLM"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("XLM", "ETH"), CurrenciesPath({"XLM", "EUR", "ETH"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("ETH", "KRW"), CurrenciesPath({"ETH", "EUR", "KRW"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("EUR", "BTC"), CurrenciesPath({"EUR", "BTC"}));
  EXPECT_EQ(exchangePublic.findCurrenciesPath("EOS", "KRW"), CurrenciesPath());
}

TEST_F(ExchangePublicTest, RetrieveMarket) {
  MarketSet markets{{"BTC", "KRW"}, {"XLM", "KRW"}, {"USD", "EOS"}};
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

  MonetaryAmount askPrice1{"0.000017 BTC"};
  MonetaryAmount bidPrice1{"0.000016 BTC"};
  MarketOrderBook marketOrderBook1{
      askPrice1, MonetaryAmount(40000, "XLM"), bidPrice1, MonetaryAmount(25000, "XLM"), volAndPriDec1, depth};

  VolAndPriNbDecimals volAndPriDec2{2, 4};

  MonetaryAmount askPrice2{"0.0063 BTC"};
  MonetaryAmount bidPrice2{"0.0062 BTC"};
  MarketOrderBook marketOrderBook2{
      askPrice2, MonetaryAmount(680, "XRP"), bidPrice2, MonetaryAmount(1479, "XRP"), volAndPriDec2, depth};

  VolAndPriNbDecimals volAndPriDec3{2, 2};

  MonetaryAmount askPrice3{"37.5 EUR"};
  MonetaryAmount bidPrice3{"37.49 EUR"};
  MarketOrderBook marketOrderBook3{
      askPrice3, MonetaryAmount("12.04 SOL"), bidPrice3, MonetaryAmount("0.45 SOL"), volAndPriDec3, depth};

  MarketOrderBookMap marketOrderBookMap{{Market("XLM", "BTC"), marketOrderBook1},
                                        {Market("XRP", "BTC"), marketOrderBook2},
                                        {Market("SOL", "EUR"), marketOrderBook3}};

  bool canUseCryptowatchAPI = false;
  PriceOptions priceOptions;
};

TEST_F(ExchangePublicConvertTest, ConvertImpossible) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"BTC"};
  MarketsPath conversionPath{};

  std::optional<MonetaryAmount> ret = exchangePublic.convert(from, toCurrency, conversionPath, fiats,
                                                             marketOrderBookMap, canUseCryptowatchAPI, priceOptions);
  ASSERT_FALSE(ret.has_value());
}

TEST_F(ExchangePublicConvertTest, ConvertSimple) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"BTC"};
  MarketsPath conversionPath{{"XLM", "BTC"}};

  std::optional<MonetaryAmount> ret = exchangePublic.convert(from, toCurrency, conversionPath, fiats,
                                                             marketOrderBookMap, canUseCryptowatchAPI, priceOptions);
  ASSERT_TRUE(ret.has_value());
  MonetaryAmount res = exchangePublic.exchangeInfo().applyFee(*marketOrderBook1.convert(from, priceOptions),
                                                              ExchangeInfo::FeeType::kMaker);
  EXPECT_EQ(*ret, res);
}

TEST_F(ExchangePublicConvertTest, ConvertDouble) {
  MonetaryAmount from{50000, "XLM"};
  CurrencyCode toCurrency{"XRP"};
  MarketsPath conversionPath{{"XLM", "BTC"}, {"XRP", "BTC"}};
  std::optional<MonetaryAmount> ret = exchangePublic.convert(from, toCurrency, conversionPath, fiats,
                                                             marketOrderBookMap, canUseCryptowatchAPI, priceOptions);
  ASSERT_TRUE(ret.has_value());
  MonetaryAmount res = exchangePublic.exchangeInfo().applyFee(*marketOrderBook1.convert(from, priceOptions),
                                                              ExchangeInfo::FeeType::kMaker);
  res = exchangePublic.exchangeInfo().applyFee(*marketOrderBook2.convert(res, priceOptions),
                                               ExchangeInfo::FeeType::kMaker);
  EXPECT_EQ(*ret, res);
}

}  // namespace cct::api