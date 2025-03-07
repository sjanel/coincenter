#include "exchangeprivateapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "accountowner.hpp"
#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cct_exception.hpp"
#include "coincenterinfo.hpp"
#include "commonapi.hpp"
#include "currencycode.hpp"
#include "default-data-dir.hpp"
#include "exchange-name-enum.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"
#include "fiatconverter.hpp"
#include "loadconfiguration.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "priceoptions.hpp"
#include "priceoptionsdef.hpp"
#include "reader.hpp"
#include "runmodes.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "tradedamounts.hpp"
#include "tradedefinitions.hpp"
#include "tradeinfo.hpp"
#include "tradeoptions.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"
#include "wallet.hpp"
#include "withdraw.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawordeposit.hpp"

namespace cct {

namespace {
BalancePortfolio addTradedAmounts(const BalancePortfolio &balancePortfolio, const TradedAmounts &tradedAmounts) {
  BalancePortfolio ret = balancePortfolio;
  ret += tradedAmounts.to;
  ret += -tradedAmounts.from;
  return ret;
}

}  // namespace

static inline bool operator==(const TradedAmountsVectorWithFinalAmount &lhs,
                              const TradedAmountsVectorWithFinalAmount &rhs) {
  return lhs.finalAmount == rhs.finalAmount && lhs.tradedAmountsVector == rhs.tradedAmountsVector;
}

static inline bool operator==(const DeliveredWithdrawInfo &lhs, const DeliveredWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}
}  // namespace cct

namespace cct::api {

static inline bool operator==(const TradeContext &lhs, const TradeContext &rhs) {
  // We don't compare on value userRef which is set from a timestamp
  return lhs.market == rhs.market && lhs.side == rhs.side;
}

static inline bool operator==(const TradeInfo &lhs, const TradeInfo &rhs) {
  return lhs.tradeContext == rhs.tradeContext && lhs.options == rhs.options;
}

class ExchangePrivateTest : public ::testing::Test {
 protected:
  void tradeBaseExpectCalls() {
    EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{market}));
  }

  TradeInfo computeTradeInfo(const TradeContext &tradeContext, const TradeOptions &tradeOptions) const {
    TradeOptions resultingTradeOptions(tradeOptions, exchangePublic.exchangeConfig().query.trade);
    return {tradeContext, resultingTradeOptions};
  }

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{settings::RunMode::kTestKeys, loadConfiguration};
  CommonAPI commonAPI{coincenterInfo, Duration::max()};

  // max to avoid real Fiat converter queries
  FiatConverter fiatConverter{coincenterInfo, Duration::max(), Reader(), Reader()};

  MockExchangePublic exchangePublic{ExchangeNameEnum::binance, fiatConverter, commonAPI, coincenterInfo};
  APIKey key{"testUser", "", "", ""};
  MockExchangePrivate exchangePrivate{exchangePublic, coincenterInfo, key};

  Market market{"ETH", "EUR"};

  VolAndPriNbDecimals volAndPriDec{2, 2};
  int depth = 15;
  TimePoint time;

  MonetaryAmount askPrice1{"2300.45 EUR"};
  MonetaryAmount bidPrice1{"2300.4 EUR"};
  MarketOrderBook marketOrderBook1{
      time, askPrice1, MonetaryAmount("1.09 ETH"), bidPrice1, MonetaryAmount("41 ETH"), volAndPriDec, depth};

  MonetaryAmount askPrice2{"2300.5 EUR"};
  MonetaryAmount bidPrice2{"2300.45 EUR"};
  MarketOrderBook marketOrderBook2{
      time, askPrice2, MonetaryAmount("7.2 ETH"), bidPrice2, MonetaryAmount("1.23 ETH"), volAndPriDec, depth};

  MonetaryAmount askPrice3{"2300.55 EUR"};
  MonetaryAmount bidPrice3{"2300.5 EUR"};
  MarketOrderBook marketOrderBook3{
      time, askPrice3, MonetaryAmount("0.96 ETH"), bidPrice3, MonetaryAmount("3.701 ETH"), volAndPriDec, depth};
};

TEST_F(ExchangePrivateTest, TakerTradeBaseToQuote) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(bidPrice1);

  PriceOptions priceOptions(PriceStrategy::taker);
  TradeOptions tradeOptions(priceOptions);
  TradeContext tradeContext(market, TradeSide::sell);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  MonetaryAmount tradedTo("23004 EUR");

  EXPECT_CALL(exchangePublic, queryOrderBook(market, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, market.quote(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, TakerTradeQuoteToBase) {
  tradeBaseExpectCalls();

  MonetaryAmount from(5000, market.quote());
  auto [_, pri] = marketOrderBook1.avgPriceAndMatchedAmountTaker(from);

  MonetaryAmount vol(from / pri, market.base());
  PriceOptions priceOptions(PriceStrategy::taker);
  TradeOptions tradeOptions(priceOptions);
  TradeContext tradeContext(market, TradeSide::buy);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  MonetaryAmount tradedTo = vol * pri.toNeutral();

  EXPECT_CALL(exchangePublic, queryOrderBook(market, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, TradeAsyncPolicyTaker) {
  tradeBaseExpectCalls();

  MonetaryAmount from(5000, market.quote());
  auto [_, pri] = marketOrderBook1.avgPriceAndMatchedAmountTaker(from);

  MonetaryAmount vol(from / pri, market.base());
  PriceOptions priceOptions(PriceStrategy::taker);
  TradeOptions tradeOptions(priceOptions, TradeTimeoutAction::cancel, TradeMode::real, seconds(10), seconds(5),
                            TradeTypePolicy::kDefault, TradeSyncPolicy::asynchronous);
  TradeContext tradeContext(market, TradeSide::buy);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  MonetaryAmount tradedTo = vol * pri.toNeutral();

  EXPECT_CALL(exchangePublic, queryOrderBook(market, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, TradeAsyncPolicyMaker) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::sell;
  TradeContext tradeContext(market, side);

  PriceOptions priceOptions(PriceStrategy::maker);
  TradeOptions tradeOptions(priceOptions, TradeTimeoutAction::cancel, TradeMode::real, seconds(10), seconds(5),
                            TradeTypePolicy::kDefault, TradeSyncPolicy::asynchronous);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  PlaceOrderInfo unmatchedPlacedOrderInfo(OrderInfo(TradedAmounts(from.currencyCode(), market.quote()), false),
                                          OrderId("Order # 0"));

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo));

  EXPECT_EQ(exchangePrivate.trade(from, market.quote(), tradeOptions), TradedAmounts(market.base(), market.quote()));
}

TEST_F(ExchangePrivateTest, MakerTradeBaseToQuote) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::sell;
  TradeContext tradeContext(market, side);

  PriceOptions priceOptions(PriceStrategy::maker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  PlaceOrderInfo unmatchedPlacedOrderInfo(OrderInfo(TradedAmounts(from.currencyCode(), market.quote()), false),
                                          OrderId("Order # 0"));

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo));

  MonetaryAmount partialMatchedFrom = from / 5;
  MonetaryAmount partialMatchedTo = partialMatchedFrom.toNeutral() * askPrice1;
  MonetaryAmount fullMatchedTo = from.toNeutral() * askPrice1;

  EXPECT_CALL(exchangePrivate, queryOrderInfo(static_cast<OrderIdView>(unmatchedPlacedOrderInfo.orderId), tradeContext))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo.orderInfo))
      .WillOnce(testing::Return(OrderInfo(TradedAmounts(partialMatchedFrom, partialMatchedTo), false)))
      .WillOnce(testing::Return(OrderInfo(TradedAmounts(from, fullMatchedTo), true)));

  EXPECT_EQ(exchangePrivate.trade(from, market.quote(), tradeOptions), TradedAmounts(from, fullMatchedTo));
}

TEST_F(ExchangePrivateTest, MakerTradeQuoteToBase) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10000, market.quote());
  MonetaryAmount pri1(bidPrice1);
  MonetaryAmount pri2(bidPrice2);
  TradeSide side = TradeSide::buy;
  TradeContext tradeContext(market, side);

  MonetaryAmount vol1(from / pri1, market.base());
  MonetaryAmount vol2(from / pri2, market.base());

  TradeOptions tradeOptions(PriceOptions{}, TradeTimeoutAction::cancel, TradeMode::real, Duration::max(),
                            Duration::zero(), TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  {
    testing::InSequence seq;

    EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(marketOrderBook1));
    EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_))
        .Times(2)
        .WillRepeatedly(testing::Return(marketOrderBook2));
    EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook3));
  }
  TradedAmounts zeroTradedAmounts(from.currencyCode(), market.base());
  OrderInfo unmatchedOrderInfo(zeroTradedAmounts, false);
  PlaceOrderInfo unmatchedPlacedOrderInfo1(unmatchedOrderInfo, OrderId("Order # 0"));
  PlaceOrderInfo unmatchedPlacedOrderInfo2(unmatchedOrderInfo, OrderId("Order # 1"));

  // Call once queryOrderBook

  // Place first order
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol1, pri1, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo1));

  EXPECT_CALL(exchangePrivate,
              queryOrderInfo(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .Times(2)
      .WillRepeatedly(testing::Return(unmatchedPlacedOrderInfo1.orderInfo));

  // Call once queryOrderBook, price still the same, no change, and queryOrderInfo a second time with unmatched amounts
  // Call once queryOrderBook with new price

  // Price change, cancel order
  EXPECT_CALL(exchangePrivate, cancelOrder(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .WillOnce(testing::Return(OrderInfo(zeroTradedAmounts, false)));

  // Place second order
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol2, pri2, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo2));

  MonetaryAmount partialMatchedFrom = from / 5;
  MonetaryAmount partialMatchedTo(partialMatchedFrom / bidPrice2, market.base());

  TradedAmounts partialMatchedTradedAmounts(partialMatchedFrom, partialMatchedTo);

  EXPECT_CALL(exchangePrivate,
              queryOrderInfo(static_cast<OrderIdView>(unmatchedPlacedOrderInfo2.orderId), tradeContext))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo2.orderInfo))
      .WillOnce(testing::Return(OrderInfo(partialMatchedTradedAmounts, false)));

  EXPECT_CALL(exchangePrivate, cancelOrder(static_cast<OrderIdView>(unmatchedPlacedOrderInfo2.orderId), tradeContext))
      .WillOnce(testing::Return(OrderInfo(partialMatchedTradedAmounts, false)));

  MonetaryAmount pri3(bidPrice3);
  MonetaryAmount vol3((from - partialMatchedFrom) / pri3, market.base());

  PlaceOrderInfo matchedPlacedOrderInfo3(OrderInfo(TradedAmounts(from - partialMatchedFrom, vol3), true),
                                         OrderId("Order # 2"));

  // Place third (and last) order, will be matched immediately
  EXPECT_CALL(exchangePrivate, placeOrder(from - partialMatchedFrom, vol3, pri3, tradeInfo))
      .WillOnce(testing::Return(matchedPlacedOrderInfo3));

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), TradedAmounts(from, partialMatchedTo + vol3));
}

TEST_F(ExchangePrivateTest, SimulatedOrderShouldNotCallPlaceOrder) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::sell;
  TradeOptions tradeOptions(PriceOptions{}, TradeTimeoutAction::cancel, TradeMode::simulation, Duration::max(),
                            Duration::zero(), TradeTypePolicy::kForceMultiTrade);
  TradeContext tradeContext(market, side);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo)).Times(0);

  // In simulation mode, fee is applied
  MonetaryAmount toAmount = exchangePublic.exchangeConfig().tradeFees.applyFee(
      from.toNeutral() * askPrice1, schema::ExchangeTradeFeesConfig::FeeType::Maker);

  EXPECT_EQ(exchangePrivate.trade(from, market.quote(), tradeOptions), TradedAmounts(from, toAmount));
}

TEST_F(ExchangePrivateTest, MakerTradeQuoteToBaseEmergencyTakerTrade) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10000, market.quote());
  MonetaryAmount pri1(bidPrice1);
  TradeSide side = TradeSide::buy;
  TradeContext tradeContext(market, side);

  MonetaryAmount vol1(from / pri1, market.base());

  TradeOptions tradeOptions(PriceOptions{}, TradeTimeoutAction::match, TradeMode::real, Duration::zero(),
                            Duration::zero(), TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_))
      .Times(2)
      .WillRepeatedly(testing::Return(marketOrderBook1));

  TradedAmounts zeroTradedAmounts(from.currencyCode(), market.base());
  OrderInfo unmatchedOrderInfo(zeroTradedAmounts, false);
  PlaceOrderInfo unmatchedPlacedOrderInfo1(unmatchedOrderInfo, OrderId("Order # 0"));

  // Place first order
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol1, pri1, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo1));

  EXPECT_CALL(exchangePrivate,
              queryOrderInfo(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo1.orderInfo));

  // Emergency reached - cancel order
  EXPECT_CALL(exchangePrivate, cancelOrder(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .WillOnce(testing::Return(OrderInfo(zeroTradedAmounts, false)));

  // Place taker order
  tradeInfo.options.switchToTakerStrategy();

  auto [_, pri2] = marketOrderBook1.avgPriceAndMatchedAmountTaker(from);
  MonetaryAmount vol2(from / pri2, market.base());

  PlaceOrderInfo matchedPlacedOrderInfo2(OrderInfo(TradedAmounts(from, vol2), true), OrderId("Order # 1"));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol2, pri2, tradeInfo))
      .WillOnce(testing::Return(matchedPlacedOrderInfo2));

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), TradedAmounts(from, vol2));
}

TEST_F(ExchangePrivateTest, MakerTradeQuoteToBaseTimeout) {
  tradeBaseExpectCalls();

  MonetaryAmount from(5000, market.quote());
  MonetaryAmount pri1(bidPrice1);
  TradeSide side = TradeSide::buy;
  TradeContext tradeContext(market, side);

  MonetaryAmount vol1(from / pri1, market.base());

  TradeOptions tradeOptions(PriceOptions{}, TradeTimeoutAction::cancel, TradeMode::real, Duration::zero(),
                            Duration::zero(), TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  TradedAmounts zeroTradedAmounts(from.currencyCode(), market.base());
  OrderInfo unmatchedOrderInfo(zeroTradedAmounts, false);
  PlaceOrderInfo unmatchedPlacedOrderInfo1(unmatchedOrderInfo, OrderId("Order # 0"));

  // Place first order, no match at place
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol1, pri1, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo1));

  MonetaryAmount partialMatchedFrom = from / 3;
  MonetaryAmount partialMatchedTo(partialMatchedFrom / bidPrice1, market.base());

  TradedAmounts partialMatchedTradedAmounts(partialMatchedFrom, partialMatchedTo);

  OrderInfo partialMatchOrderInfo(partialMatchedTradedAmounts, false);

  EXPECT_CALL(exchangePrivate,
              queryOrderInfo(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .WillOnce(testing::Return(partialMatchOrderInfo));

  // Emergency reached - cancel order
  EXPECT_CALL(exchangePrivate, cancelOrder(static_cast<OrderIdView>(unmatchedPlacedOrderInfo1.orderId), tradeContext))
      .WillOnce(testing::Return(partialMatchOrderInfo));

  // No action expected after emergency reached

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), partialMatchedTradedAmounts);
}

inline bool operator==(const InitiatedWithdrawInfo &lhs, const InitiatedWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline bool operator==(const SentWithdrawInfo &lhs, const SentWithdrawInfo &rhs) {
  return lhs.withdrawStatus() == rhs.withdrawStatus() && lhs.netEmittedAmount() == rhs.netEmittedAmount();
}

class ExchangePrivateWithdrawTest : public ExchangePrivateTest {
 protected:
  MonetaryAmount grossAmount{"2.5ETH"};
  CurrencyCode cur{grossAmount.currencyCode()};
  MockExchangePublic destinationExchangePublic{ExchangeNameEnum::kraken, fiatConverter, commonAPI, coincenterInfo};
  MockExchangePrivate destinationExchangePrivate{destinationExchangePublic, coincenterInfo, key};
  Wallet receivingWallet{
      destinationExchangePrivate.exchangeName(), cur, "TestAddress", "TestTag", WalletCheck(), AccountOwner()};

  std::string_view withdrawId = "WithdrawId";
  TimePoint withdrawTimestamp = Clock::now();
  InitiatedWithdrawInfo initiatedWithdrawInfo{receivingWallet, withdrawId, grossAmount};

  CurrencyCode currencyCode{"ETH"};
  MonetaryAmount fee{1, currencyCode, 2};
  MonetaryAmount netEmittedAmount = grossAmount - fee;
  ReceivedWithdrawInfo receivedWithdrawInfo{"deposit-id", netEmittedAmount};

  SentWithdrawInfo defaultWithdrawInfo{currencyCode};
  SentWithdrawInfo unsentWithdrawInfo{netEmittedAmount, fee, Withdraw::Status::processing};
  SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, fee, Withdraw::Status::success};
};

class ExchangePrivateWithdrawRealTest : public ExchangePrivateWithdrawTest {
 protected:
  void SetUp() override {
    EXPECT_CALL(destinationExchangePrivate, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));
    EXPECT_CALL(exchangePrivate, launchWithdraw(grossAmount, std::move(receivingWallet)))
        .WillOnce(testing::Return(initiatedWithdrawInfo));
  }

  WithdrawOptions withdrawOptions{Duration{}, WithdrawSyncPolicy::synchronous, WithdrawOptions::Mode::kReal};
};

TEST_F(ExchangePrivateWithdrawRealTest, WithdrawSynchronousReceivedAfterSent) {
  {
    testing::InSequence seq;

    EXPECT_CALL(exchangePrivate, queryRecentWithdraws(testing::_)).WillOnce(testing::Return(WithdrawsSet{}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, defaultWithdrawInfo))
        .WillOnce(testing::Return(ReceivedWithdrawInfo{}));

    EXPECT_CALL(exchangePrivate, queryRecentWithdraws(testing::_))
        .WillOnce(testing::Return(WithdrawsSet{
            Withdraw{withdrawId, withdrawTimestamp, netEmittedAmount, Withdraw::Status::processing, fee}}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, unsentWithdrawInfo))
        .WillOnce(testing::Return(ReceivedWithdrawInfo{}));

    EXPECT_CALL(exchangePrivate, queryRecentWithdraws(testing::_))
        .WillOnce(testing::Return(
            WithdrawsSet{Withdraw{withdrawId, withdrawTimestamp, netEmittedAmount, Withdraw::Status::success, fee}}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, sentWithdrawInfo))
        .Times(2)
        .WillRepeatedly(testing::Return(ReceivedWithdrawInfo{}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, sentWithdrawInfo))
        .WillOnce(testing::Return(receivedWithdrawInfo));
  }

  DeliveredWithdrawInfo deliveredWithdrawInfo(std::move(initiatedWithdrawInfo), std::move(receivedWithdrawInfo));
  EXPECT_EQ(exchangePrivate.withdraw(grossAmount, destinationExchangePrivate, withdrawOptions), deliveredWithdrawInfo);
}

TEST_F(ExchangePrivateWithdrawRealTest, WithdrawSynchronousReceivedBeforeSent) {
  {
    testing::InSequence seq;

    EXPECT_CALL(exchangePrivate, queryRecentWithdraws(testing::_)).WillOnce(testing::Return(WithdrawsSet{}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, defaultWithdrawInfo))
        .WillOnce(testing::Return(ReceivedWithdrawInfo{}));

    EXPECT_CALL(exchangePrivate, queryRecentWithdraws(testing::_)).WillOnce(testing::Return(WithdrawsSet{}));
    EXPECT_CALL(destinationExchangePrivate, queryWithdrawDelivery(initiatedWithdrawInfo, defaultWithdrawInfo))
        .WillOnce(testing::Return(receivedWithdrawInfo));
  }

  DeliveredWithdrawInfo deliveredWithdrawInfo(std::move(initiatedWithdrawInfo), std::move(receivedWithdrawInfo));
  EXPECT_EQ(exchangePrivate.withdraw(grossAmount, destinationExchangePrivate, withdrawOptions), deliveredWithdrawInfo);
}

class ExchangePrivateWithdrawSimulationTest : public ExchangePrivateWithdrawTest {
 protected:
  WithdrawOptions withdrawOptions{Duration{}, WithdrawSyncPolicy::synchronous, WithdrawOptions::Mode::kSimulation};
};

TEST_F(ExchangePrivateWithdrawSimulationTest, SimulatedWithdrawalShouldNotCallLaunchWithdraw) {
  EXPECT_CALL(destinationExchangePrivate, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));
  EXPECT_CALL(exchangePrivate, launchWithdraw(grossAmount, std::move(receivingWallet))).Times(0);

  exchangePrivate.withdraw(grossAmount, destinationExchangePrivate, withdrawOptions);
}

TEST_F(ExchangePrivateTest, WithdrawAsynchronous) {
  MonetaryAmount grossAmount("2.5ETH");
  CurrencyCode cur = grossAmount.currencyCode();
  MockExchangePublic destinationExchangePublic(ExchangeNameEnum::bithumb, fiatConverter, commonAPI, coincenterInfo);
  MockExchangePrivate destinationExchangePrivate(destinationExchangePublic, coincenterInfo, key);
  Wallet receivingWallet(destinationExchangePrivate.exchangeName(), cur, "TestAddress", "TestTag", WalletCheck(),
                         AccountOwner("SmithJohn", "스미스존"));
  EXPECT_CALL(destinationExchangePrivate, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

  InitiatedWithdrawInfo initiatedWithdrawInfo(receivingWallet, "WithdrawId", grossAmount);
  EXPECT_CALL(exchangePrivate, launchWithdraw(grossAmount, std::move(receivingWallet)))
      .WillOnce(testing::Return(initiatedWithdrawInfo));

  DeliveredWithdrawInfo deliveredWithdrawInfo(std::move(initiatedWithdrawInfo), ReceivedWithdrawInfo{});

  EXPECT_EQ(exchangePrivate.withdraw(
                grossAmount, destinationExchangePrivate,
                WithdrawOptions(Duration{}, WithdrawSyncPolicy::asynchronous, WithdrawOptions::Mode::kReal)),
            deliveredWithdrawInfo);
}

class ExchangePrivateDustSweeperTest : public ExchangePrivateTest {
 protected:
  ExchangePrivateDustSweeperTest() {
    EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
  }

  void expectQueryTradableMarkets() {
    EXPECT_CALL(exchangePublic, queryTradableMarkets())
        .WillOnce(testing::Return(MarketSet{xrpbtcMarket, xrpeurMarket, Market{"ETH", "EUR"}}));
  }

  void expectMarketOrderBookCall(Market mk, int nTimes = 1) {
    if (mk == xrpbtcMarket) {
      EXPECT_CALL(exchangePublic, queryOrderBook(mk, MarketOrderBook::kDefaultDepth))
          .Times(nTimes)
          .WillRepeatedly(testing::Return(xrpbtcMarketOrderBook));
    } else if (mk == xrpeurMarket) {
      EXPECT_CALL(exchangePublic, queryOrderBook(mk, MarketOrderBook::kDefaultDepth))
          .Times(nTimes)
          .WillRepeatedly(testing::Return(xrpeurMarketOrderBook));
    } else {
      throw exception("Invalid market");
    }
  }

  TradedAmounts expectTakerSell(MonetaryAmount from, MonetaryAmount pri, int percentageSold = 100) {
    MonetaryAmount vol(from);

    Market mk{from.currencyCode(), pri.currencyCode()};
    TradeContext tradeContext(mk, TradeSide::sell);
    TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

    MonetaryAmount tradedTo = vol.toNeutral() * pri;

    TradedAmounts tradedAmounts;
    if (percentageSold == 100) {
      tradedAmounts = TradedAmounts(from, tradedTo);  // to avoid rounding issues
    } else {
      tradedAmounts = TradedAmounts((from * percentageSold) / 100, (tradedTo * percentageSold) / 100);
    }

    OrderId orderId{"OrderId # "};
    AppendIntegralToString(orderId, orderIdInt++);

    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
        .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(tradedAmounts, true), orderId)));

    return tradedAmounts;
  }

  TradedAmounts expectTakerBuy(MonetaryAmount to, MonetaryAmount askPri, MonetaryAmount bidPri, Market mk,
                               bool success = true) {
    MonetaryAmount from = to.toNeutral() * bidPri;
    MonetaryAmount vol(from / askPri, mk.base());

    TradeContext tradeContext(mk, TradeSide::buy);
    TradeInfo tradeInfo = computeTradeInfo(tradeContext, tradeOptions);

    TradedAmounts tradedAmounts(MonetaryAmount{success ? from : MonetaryAmount(0), askPri.currencyCode()},
                                success ? vol : MonetaryAmount{0, vol.currencyCode()});

    OrderId orderId{"OrderId # "};
    AppendIntegralToString(orderId, orderIdInt++);

    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, askPri, tradeInfo))
        .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(tradedAmounts, true), orderId)));

    return tradedAmounts;
  }

  void expectMarketPricesMapCall() {
    EXPECT_CALL(exchangePublic, queryAllPrices()).WillOnce(testing::Return(marketPriceMap));
  }

  std::optional<MonetaryAmount> dustThreshold(CurrencyCode cur) {
    const auto &dustThresholds = exchangePublic.exchangeConfig().query.dustAmountsThreshold;
    auto dustThresholdLb = dustThresholds.find(MonetaryAmount(0, cur));
    if (dustThresholdLb == dustThresholds.end()) {
      return std::nullopt;
    }
    return *dustThresholdLb;
  }

  int orderIdInt = 0;

  CurrencyCode dustCur{"XRP"};

  Market xrpbtcMarket{dustCur, "BTC"};
  Market xrpeurMarket{dustCur, "EUR"};
  Market etheurMarket{"ETH", "EUR"};

  PriceOptions priceOptions{PriceStrategy::taker};
  TradeOptions tradeOptions{priceOptions};

  MonetaryAmount xrpbtcBidPri{31, "BTC", 6};
  MonetaryAmount xrpbtcAskPri{32, "BTC", 6};
  MarketOrderBook xrpbtcMarketOrderBook{
      time, xrpbtcAskPri, MonetaryAmount(40, dustCur), xrpbtcBidPri, MonetaryAmount(27909, dustCur, 3), {3, 6}, depth};

  MonetaryAmount xrpeurBidPri{5, "EUR", 1};
  MonetaryAmount xrpeurAskPri{51, "EUR", 2};
  MarketOrderBook xrpeurMarketOrderBook{
      time, xrpeurAskPri, MonetaryAmount(40, dustCur), xrpeurBidPri, MonetaryAmount(27909, dustCur, 3), {3, 2}, depth};

  MonetaryAmount xrpethBidPri{134567, "EUR", 2};

  MarketPriceMap marketPriceMap{
      {xrpbtcMarket, xrpbtcBidPri}, {xrpeurMarket, xrpeurBidPri}, {etheurMarket, xrpethBidPri}};

  BalanceOptions balanceOptions;
};

TEST_F(ExchangePrivateDustSweeperTest, DustSweeperNoThreshold) {
  auto actualRes = exchangePrivate.queryDustSweeper("ETC");
  EXPECT_TRUE(actualRes.tradedAmountsVector.empty());
  EXPECT_TRUE(actualRes.finalAmount.isDefault());
}

TEST_F(ExchangePrivateDustSweeperTest, DustSweeperHigherThanThresholdNoAction) {
  EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{market}));

  MonetaryAmount dustCurAmount{1, dustCur};
  BalancePortfolio balance{dustCurAmount};
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance));

  auto actualRes = exchangePrivate.queryDustSweeper(dustCur);
  EXPECT_TRUE(actualRes.tradedAmountsVector.empty());
  EXPECT_EQ(actualRes.finalAmount, dustCurAmount);
}

TEST_F(ExchangePrivateDustSweeperTest, DustSweeperDirectSellingPossible) {
  // Scenario:
  // - try to sell all XRP at once, it succeeds
  expectQueryTradableMarkets();

  MonetaryAmount from(4, xrpbtcMarket.base(), 1);
  MonetaryAmount pri(xrpbtcBidPri);

  TradedAmounts tradedAmounts = expectTakerSell(from, pri);

  expectMarketOrderBookCall(xrpbtcMarket);

  MonetaryAmount avBtcAmount{75, "BTC", 4};
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(BalancePortfolio{from, avBtcAmount}))
      .WillOnce(testing::Return(BalancePortfolio{avBtcAmount + tradedAmounts.to}));

  TradedAmountsVector tradedAmountsVector{tradedAmounts};
  TradedAmountsVectorWithFinalAmount res{tradedAmountsVector, MonetaryAmount{0, dustCur}};
  EXPECT_EQ(exchangePrivate.queryDustSweeper(dustCur), res);
}

TEST_F(ExchangePrivateDustSweeperTest, DustSweeper2StepsSameMarket) {
  // Scenario:
  // - try to sell directly XRP into BTC, it fails
  // - try to buy some XRP from BTC, it succeeds
  // - try to sell all XRP at once, it succeeds
  expectQueryTradableMarkets();

  MonetaryAmount from(4, xrpbtcMarket.base(), 1);
  MonetaryAmount pri(xrpbtcBidPri);

  expectMarketPricesMapCall();

  expectMarketOrderBookCall(xrpbtcMarket, 3);

  expectTakerSell(from, pri, 0);  // no selling possible

  MonetaryAmount xrpDustThreshold = dustThreshold(dustCur).value_or(MonetaryAmount{-1});
  TradedAmounts tradedAmounts1 = expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket);

  TradedAmounts tradedAmounts2 = expectTakerSell(from + tradedAmounts1.to, pri);

  MonetaryAmount avBtcAmount{75, "BTC", 4};
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(BalancePortfolio{from, avBtcAmount}))
      .WillOnce(testing::Return(BalancePortfolio{from + tradedAmounts1.to, avBtcAmount - tradedAmounts1.from}))
      .WillOnce(testing::Return(BalancePortfolio{avBtcAmount - tradedAmounts1.from}));

  TradedAmountsVector tradedAmountsVector{tradedAmounts1, tradedAmounts2};
  TradedAmountsVectorWithFinalAmount res{tradedAmountsVector, MonetaryAmount{0, dustCur}};
  EXPECT_EQ(exchangePrivate.queryDustSweeper(dustCur), res);
}

TEST_F(ExchangePrivateDustSweeperTest, DustSweeper5Steps) {
  // Scenario:
  // - try to sell directly XRP into BTC, it fails
  // - try to sell directly XRP into EUR, it fails
  // - try to buy some XRP from BTC, it succeeds
  // - try to sell all XRP at once into BTC, only 80 % are sold
  // - try to sell all XRP at once into EUR, it fails
  // - try to sell all XRP at once into BTC, it fails
  // - try to buy some XRP from EUR, it fails at first with multiplier 1x
  // - try to buy some XRP from BTC, it fails at first with multiplier 1x
  // - try to buy some XRP from EUR, it succeeds with multiplier at 1.5x
  // - try to sell all XRP at once in EUR, it succeeds
  expectQueryTradableMarkets();

  MonetaryAmount from(4, xrpbtcMarket.base(), 1);
  MonetaryAmount priBtc(xrpbtcBidPri);
  MonetaryAmount priEur(xrpeurBidPri);

  expectMarketPricesMapCall();

  expectMarketOrderBookCall(xrpbtcMarket, 5);
  expectMarketOrderBookCall(xrpeurMarket, 5);

  ::testing::InSequence inSeq;

  MonetaryAmount avBtcAmount{75, "BTC", 4};
  MonetaryAmount avEurAmount{500, "EUR"};

  BalancePortfolio balance1{from, avBtcAmount, avEurAmount};

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance1));

  // BTC should be called first because markets are lexicographically sorted
  expectTakerSell(from, priBtc, 0);  // no selling possible
  expectTakerSell(from, priEur, 0);  // no selling possible

  MonetaryAmount xrpDustThreshold = dustThreshold(dustCur).value_or(MonetaryAmount{-1});
  TradedAmounts tradedAmounts1 = expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket);
  from += tradedAmounts1.to;

  BalancePortfolio balance2 = addTradedAmounts(balance1, tradedAmounts1);

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance2));

  int percentXRPSoldSecondStep = 80;
  TradedAmounts tradedAmounts2 = expectTakerSell(from, priBtc, percentXRPSoldSecondStep);
  from -= tradedAmounts2.from;

  BalancePortfolio balance3 = addTradedAmounts(balance2, tradedAmounts2);

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance3));

  expectTakerSell(from, priEur, 0);  // no selling possible
  expectTakerSell(from, priBtc, 0);  // no selling possible

  // try buy with multiplier 1x, not possible
  expectTakerBuy(xrpDustThreshold, xrpeurAskPri, xrpeurBidPri, xrpeurMarket, false);
  expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket, false);

  TradedAmounts tradedAmounts3 = expectTakerBuy((3 * xrpDustThreshold) / 2, xrpeurAskPri, xrpeurBidPri, xrpeurMarket);
  from += tradedAmounts3.to;

  BalancePortfolio balance4 = addTradedAmounts(balance3, tradedAmounts3);
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance4));

  // Final sell that works
  TradedAmounts tradedAmounts4 = expectTakerSell(from, priEur);

  BalancePortfolio balance5 = addTradedAmounts(balance4, tradedAmounts4);

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance5));

  TradedAmountsVector tradedAmountsVector{tradedAmounts1, tradedAmounts2, tradedAmounts3, tradedAmounts4};
  TradedAmountsVectorWithFinalAmount res{tradedAmountsVector, MonetaryAmount{0, dustCur}};
  EXPECT_EQ(exchangePrivate.queryDustSweeper(dustCur), res);
}

}  // namespace cct::api