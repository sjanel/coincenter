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
  ExchangePublicTest()
      : cryptowatchAPI(coincenterInfo),
        fiatConverter(coincenterInfo, Duration::max()),  // max to avoid real Fiat converter queries
        exchangePublic(kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  MockExchangePublic exchangePublic;
};

namespace {
using MarketsPath = ExchangePublic::MarketsPath;
using CurrenciesPath = ExchangePublic::CurrenciesPath;
using MarketSet = ExchangePublic::MarketSet;
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
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).Times(3).WillRepeatedly(::testing::Return(markets));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC"), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS"), exception);
}

}  // namespace cct::api