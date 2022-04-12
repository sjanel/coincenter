#include "exchangeprivateapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"

namespace cct {
inline bool operator==(const WithdrawInfo &lhs, const WithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}
}  // namespace cct

namespace cct::api {

class ExchangePrivateTest : public ::testing::Test {
 protected:
  ExchangePrivateTest() = default;

  virtual void SetUp() {}

  virtual void TearDown() {}

  void tradeBaseExpectCalls() {
    EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{m}));
  }

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{settings::RunMode::kProd, loadConfiguration};
  CryptowatchAPI cryptowatchAPI{coincenterInfo, settings::RunMode::kProd, Duration::max(), true};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  MockExchangePublic exchangePublic{kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo};
  APIKey key{"test", "testuser", "", "", ""};
  MockExchangePrivate exchangePrivate{exchangePublic, coincenterInfo, key};

  Market m{"ETH", "EUR"};

  VolAndPriNbDecimals volAndPriDec{2, 2};
  int depth = 15;
  int64_t nbSecondsSinceEpoch = 0;

  MonetaryAmount askPrice1{"2300.45 EUR"};
  MonetaryAmount bidPrice1{"2300.4 EUR"};
  MarketOrderBook marketOrderBook1{
      askPrice1, MonetaryAmount("1.09 ETH"), bidPrice1, MonetaryAmount("41 ETH"), volAndPriDec, depth};

  MonetaryAmount askPrice2{"2300.5 EUR"};
  MonetaryAmount bidPrice2{"2300.45 EUR"};
  MarketOrderBook marketOrderBook2{
      askPrice2, MonetaryAmount("7.2 ETH"), bidPrice2, MonetaryAmount("1.23 ETH"), volAndPriDec, depth};

  MonetaryAmount askPrice3{"2300.55 EUR"};
  MonetaryAmount bidPrice3{"2300.5 EUR"};
  MarketOrderBook marketOrderBook3{
      askPrice3, MonetaryAmount("0.96 ETH"), bidPrice3, MonetaryAmount("3.701 ETH"), volAndPriDec, depth};
};

inline bool operator==(const TradeInfo &lhs, const TradeInfo &rhs) { return lhs.m == rhs.m && lhs.side == rhs.side; }

inline bool operator==(const OrderRef &lhs, const OrderRef &rhs) { return lhs.id == rhs.id; }

TEST_F(ExchangePrivateTest, TakerTradeBaseToQuote) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, m.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(bidPrice1);

  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, TradeSide::kSell, tradeOptions);

  MonetaryAmount tradedTo("23004 EUR");

  EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, m.quote(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, TakerTradeQuoteToBase) {
  tradeBaseExpectCalls();

  MonetaryAmount from(5000, m.quote());
  MonetaryAmount pri(*marketOrderBook1.computeAvgPriceForTakerAmount(from));

  MonetaryAmount vol(from / pri, m.base());
  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, TradeSide::kBuy, tradeOptions);

  MonetaryAmount tradedTo = vol * pri.toNeutral();

  EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, m.base(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, MakerTradeBaseToQuote) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, m.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::kSell;
  PriceOptions priceOptions(PriceStrategy::kMaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, side, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(m, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  PlaceOrderInfo unmatchedPlacedOrderInfo(OrderInfo(TradedAmounts(from.currencyCode(), m.quote()), false),
                                          OrderId("Order # 0"));

  OrderRef orderRef(unmatchedPlacedOrderInfo.orderId, nbSecondsSinceEpoch, m, side);

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo));

  MonetaryAmount partialMatchedFrom = from / 5;
  MonetaryAmount partialMatchedTo = partialMatchedFrom.toNeutral() * askPrice1;
  MonetaryAmount fullMatchedTo = from.toNeutral() * askPrice1;

  EXPECT_CALL(exchangePrivate, queryOrderInfo(orderRef))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo.orderInfo))
      .WillOnce(testing::Return(OrderInfo(TradedAmounts(partialMatchedFrom, partialMatchedTo), false)))
      .WillOnce(testing::Return(OrderInfo(TradedAmounts(from, fullMatchedTo), true)));

  EXPECT_EQ(exchangePrivate.trade(from, m.quote(), tradeOptions), TradedAmounts(from, fullMatchedTo));
}

TEST_F(ExchangePrivateTest, MakerTradeQuoteToBase) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10000, m.quote());
  MonetaryAmount pri1(bidPrice1);
  MonetaryAmount pri2(bidPrice2);
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount vol1(from / pri1, m.base());
  MonetaryAmount vol2(from / pri2, m.base());

  TradeOptions tradeOptions(TradeTimeoutAction::kCancel, TradeMode::kReal, Duration::max(), Duration::zero(),
                            TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, side, tradeOptions);

  {
    testing::InSequence seq;

    EXPECT_CALL(exchangePublic, queryOrderBook(m, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(marketOrderBook1));
    EXPECT_CALL(exchangePublic, queryOrderBook(m, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(marketOrderBook2));
    EXPECT_CALL(exchangePublic, queryOrderBook(m, testing::_)).WillOnce(testing::Return(marketOrderBook3));
  }
  TradedAmounts zeroTradedAmounts(from.currencyCode(), m.base());
  OrderInfo unmatchedOrderInfo(zeroTradedAmounts, false);
  PlaceOrderInfo unmatchedPlacedOrderInfo1(unmatchedOrderInfo, OrderId("Order # 0"));
  PlaceOrderInfo unmatchedPlacedOrderInfo2(unmatchedOrderInfo, OrderId("Order # 1"));

  OrderRef orderRef1(unmatchedPlacedOrderInfo1.orderId, nbSecondsSinceEpoch, m, side);
  OrderRef orderRef2(unmatchedPlacedOrderInfo2.orderId, nbSecondsSinceEpoch, m, side);

  // Call once queryOrderBook

  // Place first order
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol1, pri1, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo1));

  EXPECT_CALL(exchangePrivate, queryOrderInfo(orderRef1))
      .Times(2)
      .WillRepeatedly(testing::Return(unmatchedPlacedOrderInfo1.orderInfo));

  // Call once queryOrderBook, price still the same, no change, and queryOrderInfo a second time with unmatched amounts
  // Call once queryOrderBook with new price

  // Price change, cancel order
  EXPECT_CALL(exchangePrivate, cancelOrder(orderRef1)).WillOnce(testing::Return(OrderInfo(zeroTradedAmounts, false)));

  // Place second order
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol2, pri2, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo2));

  MonetaryAmount partialMatchedFrom = from / 5;
  MonetaryAmount partialMatchedTo(partialMatchedFrom / bidPrice2, m.base());

  TradedAmounts partialMatchedTradedAmounts(partialMatchedFrom, partialMatchedTo);

  EXPECT_CALL(exchangePrivate, queryOrderInfo(orderRef2))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo2.orderInfo))
      .WillOnce(testing::Return(OrderInfo(partialMatchedTradedAmounts, false)));

  EXPECT_CALL(exchangePrivate, cancelOrder(orderRef2))
      .WillOnce(testing::Return(OrderInfo(partialMatchedTradedAmounts, false)));

  MonetaryAmount pri3(bidPrice3);
  MonetaryAmount vol3((from - partialMatchedFrom) / pri3, m.base());

  PlaceOrderInfo matchedPlacedOrderInfo3(OrderInfo(TradedAmounts(from - partialMatchedFrom, vol3), true),
                                         OrderId("Order # 2"));

  // Place third (and last) order, will be matched immediately
  EXPECT_CALL(exchangePrivate, placeOrder(from - partialMatchedFrom, vol3, pri3, tradeInfo))
      .WillOnce(testing::Return(matchedPlacedOrderInfo3));

  EXPECT_EQ(exchangePrivate.trade(from, m.base(), tradeOptions), TradedAmounts(from, partialMatchedTo + vol3));
}

TEST_F(ExchangePrivateTest, SimulatedOrderShouldNotCallPlaceOrder) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, m.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::kSell;
  TradeOptions tradeOptions(TradeTimeoutAction::kCancel, TradeMode::kSimulation, Duration::max(), Duration::zero(),
                            TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, side, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(m, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo)).Times(0);

  // In simulation mode, fee is applied
  MonetaryAmount toAmount =
      exchangePublic.exchangeInfo().applyFee(from.toNeutral() * askPrice1, ExchangeInfo::FeeType::kMaker);

  EXPECT_EQ(exchangePrivate.trade(from, m.quote(), tradeOptions), TradedAmounts(from, toAmount));
}

inline bool operator==(const InitiatedWithdrawInfo &lhs, const InitiatedWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline bool operator==(const SentWithdrawInfo &lhs, const SentWithdrawInfo &rhs) {
  return lhs.isWithdrawSent() == rhs.isWithdrawSent() && lhs.netEmittedAmount() == rhs.netEmittedAmount();
}

TEST_F(ExchangePrivateTest, Withdraw) {
  MonetaryAmount grossAmount("2.5ETH");
  CurrencyCode cur = grossAmount.currencyCode();
  MockExchangePublic destinationExchangePublic("bithumb", fiatConverter, cryptowatchAPI, coincenterInfo);
  MockExchangePrivate destinationExchangePrivate(destinationExchangePublic, coincenterInfo, key);
  std::string_view address = "TestAddress";
  std::string_view tag = "TestTag";
  Wallet receivingWallet(destinationExchangePrivate.exchangeName(), cur, address, tag, WalletCheck());
  EXPECT_CALL(destinationExchangePrivate, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

  WithdrawIdView withdrawIdView = "WithdrawId";
  InitiatedWithdrawInfo initiatedWithdrawInfo(receivingWallet, withdrawIdView, grossAmount);
  EXPECT_CALL(exchangePrivate, launchWithdraw(grossAmount, std::move(receivingWallet)))
      .WillOnce(testing::Return(initiatedWithdrawInfo));

  MonetaryAmount fee("0.01 ETH");
  MonetaryAmount netEmittedAmount = grossAmount - fee;

  SentWithdrawInfo unsentWithdrawInfo(netEmittedAmount, false);
  SentWithdrawInfo sentWithdrawInfo(netEmittedAmount, true);
  {
    testing::InSequence seq;

    EXPECT_CALL(exchangePrivate, isWithdrawSuccessfullySent(initiatedWithdrawInfo))
        .Times(2)
        .WillRepeatedly(testing::Return(unsentWithdrawInfo));
    EXPECT_CALL(exchangePrivate, isWithdrawSuccessfullySent(initiatedWithdrawInfo))
        .WillOnce(testing::Return(sentWithdrawInfo));
  }

  {
    testing::InSequence seq;

    EXPECT_CALL(destinationExchangePrivate, isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo))
        .Times(2)
        .WillRepeatedly(testing::Return(false));
    EXPECT_CALL(destinationExchangePrivate, isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo))
        .WillOnce(testing::Return(true));
  }

  WithdrawInfo withdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
  EXPECT_EQ(exchangePrivate.withdraw(grossAmount, destinationExchangePrivate, Duration::zero()), withdrawInfo);
}

}  // namespace cct::api