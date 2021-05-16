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
  MockExchangePublic(std::string_view name, FiatConverter &fiatConverter, CryptowatchAPI &cryptowatchApi)
      : ExchangePublic(name, fiatConverter, cryptowatchApi) {}

  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(CurrencyExchange, convertStdCurrencyToCurrencyExchange, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketSet, queryTradableMarkets, (), (override));
  MOCK_METHOD(MarketPriceMap, queryAllPrices, (), (override));
  MOCK_METHOD(WithdrawalFeeMap, queryWithdrawalFees, (), (override));
  MOCK_METHOD(MonetaryAmount, queryWithdrawalFees, (CurrencyCode currencyCode), (override));
  MOCK_METHOD(MarketOrderBookMap, queryAllApproximatedOrderBooks, (int depth), (override));
  MOCK_METHOD(MarketOrderBook, queryOrderBook, (Market m, int depth), (override));
};

class ExchangePublicTest : public ::testing::Test {
 protected:
  ExchangePublicTest() : exchangePublic("test", fiatConverter, cryptowatchAPI) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  MockExchangePublic exchangePublic;
};

TEST_F(ExchangePublicTest, FindFastestConversionPath) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(4)
      .WillRepeatedly(
          ::testing::Return(ExchangePublic::MarketSet{{"BTC", "EUR"}, {"XLM", "EUR"}, {"ETH", "EUR"}, {"ETH", "BTC"}}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("BTC", "XLM"), ExchangePublic::Currencies({"BTC", "EUR", "XLM"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("XLM", "ETH"), ExchangePublic::Currencies({"XLM", "EUR", "ETH"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("ETH", "KRW"), ExchangePublic::Currencies({"ETH", "EUR", "KRW"}));
  EXPECT_EQ(exchangePublic.findFastestConversionPath("EOS", "KRW"), ExchangePublic::Currencies());
}

TEST_F(ExchangePublicTest, RetriveMarket) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets())
      .Times(3)
      .WillRepeatedly(::testing::Return(ExchangePublic::MarketSet{{"BTC", "KRW"}, {"XLM", "KRW"}, {"USD", "EOS"}}));

  EXPECT_EQ(exchangePublic.retrieveMarket("BTC", "KRW"), Market("BTC", "KRW"));
  EXPECT_EQ(exchangePublic.retrieveMarket("KRW", "BTC"), Market("BTC", "KRW"));
  EXPECT_THROW(exchangePublic.retrieveMarket("EUR", "EOS"), cct::exception);
}

}  // namespace api
}  // namespace cct