#include "exchangepublicapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_exception.hpp"
#include "cryptowatchapi.hpp"
#include "fiatconverter.hpp"

namespace cct {
namespace api {
class MockExchangePublic : public ExchangePublic {
 public:
  MockExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi,
                     const CoincenterInfo &config)
      : ExchangePublic(name, fiatConverter, cryptowatchApi, config) {}

  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(CurrencyExchange, convertStdCurrencyToCurrencyExchange, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketSet, queryTradableMarkets, (), (override));
  MOCK_METHOD(MarketPriceMap, queryAllPrices, (), (override));
  MOCK_METHOD(WithdrawalFeeMap, queryWithdrawalFees, (), (override));
  MOCK_METHOD(MonetaryAmount, queryWithdrawalFee, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketOrderBookMap, queryAllApproximatedOrderBooks, (int depth), (override));
  MOCK_METHOD(MarketOrderBook, queryOrderBook, (Market m, int depth), (override));
};

class ExchangePublicTest : public ::testing::Test {
 protected:
  ExchangePublicTest() : exchangePublic("test", fiatConverter, cryptowatchAPI, coincenterInfo) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  CoincenterInfo coincenterInfo;
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

TEST_F(ExchangePublicTest, RetriveMarket) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(3)
      .WillRepeatedly(::testing::Return(MarketSet{{"BTC", "KRW"}, {"XLM", "KRW"}, {"USD", "EOS"}}));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC"), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS"), cct::exception);
}

}  // namespace api
}  // namespace cct