#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_exception.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "mock_exchangepublicapi.hpp"

namespace cct::api {
class ExchangePublicTest : public ::testing::Test {
 protected:
  ExchangePublicTest()
      : cryptowatchAPI(coincenterInfo),
        fiatConverter(coincenterInfo),
        exchangePublic("test", fiatConverter, cryptowatchAPI, coincenterInfo) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  MockExchangePublic exchangePublic;
};

namespace {
using ConversionPath = ExchangePublic::ConversionPath;
using MarketSet = ExchangePublic::MarketSet;
}  // namespace

TEST_F(ExchangePublicTest, FindFastestConversionPath) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(4)
      .WillRepeatedly(::testing::Return(MarketSet{{"BTC", "EUR"}, {"XLM", "EUR"}, {"ETH", "EUR"}, {"ETH", "BTC"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("BTC", "XLM"),
            ConversionPath({Market{"BTC", "EUR"}, Market{"XLM", "EUR"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("XLM", "ETH"),
            ConversionPath({Market{"XLM", "EUR"}, Market{"ETH", "EUR"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("ETH", "KRW"),
            ConversionPath({Market{"ETH", "EUR"}, Market{"EUR", "KRW"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("EOS", "KRW"), ConversionPath());
}

TEST_F(ExchangePublicTest, RetrieveMarket) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(3)
      .WillRepeatedly(::testing::Return(MarketSet{{"BTC", "KRW"}, {"XLM", "KRW"}, {"USD", "EOS"}}));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC"), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS"), exception);
}

}  // namespace cct::api