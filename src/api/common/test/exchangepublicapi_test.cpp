#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_exception.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"
#include "mock_exchangepublicapi.hpp"

namespace cct {
namespace api {
class ExchangePublicTest : public ::testing::Test {
 protected:
  ExchangePublicTest()
      : cryptowatchAPI(coincenterInfo),
        fiatConverter(coincenterInfo.dataDir()),
        exchangePublic("test", fiatConverter, cryptowatchAPI, coincenterInfo) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  MockExchangePublic exchangePublic;
};

namespace {
using Currencies = ExchangePublic::Currencies;
using MarketSet = ExchangePublic::MarketSet;
}  // namespace

TEST_F(ExchangePublicTest, FindFastestConversionPath) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(4)
      .WillRepeatedly(::testing::Return(MarketSet{{"BTC", "EUR"}, {"XLM", "EUR"}, {"ETH", "EUR"}, {"ETH", "BTC"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath(Market("BTC", "XLM")), Currencies({"BTC", "EUR", "XLM"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath(Market("XLM", "ETH")), Currencies({"XLM", "EUR", "ETH"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath(Market("ETH", "KRW")), Currencies({"ETH", "EUR", "KRW"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath(Market("EOS", "KRW")), Currencies());
}

TEST_F(ExchangePublicTest, RetrieveMarket) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(3)
      .WillRepeatedly(::testing::Return(MarketSet{{"BTC", "KRW"}, {"XLM", "KRW"}, {"USD", "EOS"}}));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC"), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS"), exception);
}

}  // namespace api
}  // namespace cct