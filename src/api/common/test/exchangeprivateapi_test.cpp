#include "exchangeprivateapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "mock_exchangepublicapi.hpp"

namespace cct::api {
class MockExchangePrivate : public ExchangePrivate {
 public:
  MockExchangePrivate(ExchangePublic &exchangePublic, const CoincenterInfo &config, const APIKey &apiKey)
      : ExchangePrivate(exchangePublic, config, apiKey) {}

  MOCK_METHOD(CurrencyExchangeFlatSet, queryTradableCurrencies, (), (override));
  MOCK_METHOD(BalancePortfolio, queryAccountBalance, (CurrencyCode equiCurrency), (override));
  MOCK_METHOD(Wallet, queryDepositWallet, (CurrencyCode currencyCode), (override));

  MOCK_METHOD(bool, isSimulatedOrderSupported, (), (const override));

  MOCK_METHOD(PlaceOrderInfo, placeOrder,
              (MonetaryAmount from, MonetaryAmount volume, MonetaryAmount price, const TradeInfo &tradeInfo),
              (override));
  MOCK_METHOD(OrderInfo, cancelOrder, (const OrderId &orderId, const TradeInfo &tradeInfo), (override));
  MOCK_METHOD(OrderInfo, queryOrderInfo, (const OrderId &orderId, const TradeInfo &tradeInfo), (override));
  MOCK_METHOD(InitiatedWithdrawInfo, launchWithdraw, (MonetaryAmount grossAmount, Wallet &&wallet), (override));
  MOCK_METHOD(SentWithdrawInfo, isWithdrawSuccessfullySent, (const InitiatedWithdrawInfo &initiatedWithdrawInfo),
              (override));
  MOCK_METHOD(bool, isWithdrawReceived,
              (const InitiatedWithdrawInfo &initiatedWithdrawInfo, const SentWithdrawInfo &sentWithdrawInfo),
              (override));
};

class ExchangePrivateTest : public ::testing::Test {
 protected:
  ExchangePrivateTest()
      : cryptowatchAPI(coincenterInfo),
        fiatConverter(coincenterInfo),
        exchangePublic("kraken", fiatConverter, cryptowatchAPI, coincenterInfo),
        key("test", "testuser", "", "", ""),
        exchangePrivate(exchangePublic, coincenterInfo, key) {}

  virtual void SetUp() {}
  virtual void TearDown() {}

  CoincenterInfo coincenterInfo;
  CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  MockExchangePublic exchangePublic;
  APIKey key;
  MockExchangePrivate exchangePrivate;

  Market m{"eth", "eur"};
  MonetaryAmount askPrice{"2300.45 EUR"};
  MonetaryAmount bidPrice{"2300.4 EUR"};
  MarketOrderBook marketOrderBook{askPrice, MonetaryAmount("1.09 ETH"), bidPrice, MonetaryAmount("41 ETH"), {2, 2}, 15};
};

namespace {
using MarketSet = ExchangePublic::MarketSet;
}  // namespace

inline bool operator==(const TradeInfo &lhs, const TradeInfo &rhs) {
  return lhs.fromCurrencyCode == rhs.fromCurrencyCode && lhs.toCurrencyCode == rhs.toCurrencyCode && lhs.m == rhs.m;
}

inline bool operator==(const TradedAmounts &lhs, const TradedAmounts &rhs) {
  return lhs.tradedFrom == rhs.tradedFrom && lhs.tradedTo == rhs.tradedTo;
}

inline bool operator==(const OrderInfo &lhs, const OrderInfo &rhs) {
  return lhs.isClosed == rhs.isClosed && lhs.tradedAmounts == rhs.tradedAmounts;
}

inline bool operator==(const PlaceOrderInfo &lhs, const PlaceOrderInfo &rhs) {
  return lhs.orderInfo == rhs.orderInfo && lhs.orderId == rhs.orderId;
}

TEST_F(ExchangePrivateTest, TakerTradeBaseToQuote) {
  MonetaryAmount from(10, m.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(bidPrice);

  TradeOptions tradeOptions(TradePriceStrategy::kTaker);
  TradeInfo tradeInfo(m.base(), m.quote(), m, tradeOptions, 0);

  MonetaryAmount tradedTo("23004 EUR");

  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{m}));
  EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true))));

  EXPECT_EQ(exchangePrivate.trade(from, m.quote(), tradeOptions), tradedTo);
}

TEST_F(ExchangePrivateTest, TakerTradeQuoteToBase) {
  MonetaryAmount from(5000, m.quote());
  MonetaryAmount pri(*marketOrderBook.computeAvgPriceForTakerAmount(from));

  MonetaryAmount vol(from / pri, m.base());
  TradeOptions tradeOptions(TradePriceStrategy::kTaker);
  TradeInfo tradeInfo(m.quote(), m.base(), m, tradeOptions, 0);

  MonetaryAmount tradedTo = vol * pri.toNeutral();

  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{m}));
  EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true))));

  EXPECT_EQ(exchangePrivate.trade(from, m.base(), tradeOptions), tradedTo);
}

}  // namespace cct::api