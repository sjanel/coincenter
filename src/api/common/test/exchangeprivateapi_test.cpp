#include "exchangeprivateapi.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "coincenterinfo.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"
#include "exchangepublicapitypes.hpp"
#include "stringhelpers.hpp"

namespace cct {
inline bool operator==(const WithdrawInfo &lhs, const WithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline BalancePortfolio operator+(const BalancePortfolio &b, const TradedAmounts &a) {
  BalancePortfolio ret = b;
  ret.add(a.tradedTo);
  ret.add(-a.tradedFrom);
  return ret;
}

inline bool operator==(const TradedAmountsVectorWithFinalAmount &lhs, const TradedAmountsVectorWithFinalAmount &rhs) {
  return lhs.finalAmount == rhs.finalAmount && lhs.tradedAmountsVector == rhs.tradedAmountsVector;
}
}  // namespace cct

namespace cct::api {

class ExchangePrivateTest : public ::testing::Test {
 protected:
  void tradeBaseExpectCalls() {
    EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(MarketSet{market}));
  }

  LoadConfiguration loadConfiguration{kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest};
  CoincenterInfo coincenterInfo{settings::RunMode::kProd, loadConfiguration};
  CryptowatchAPI cryptowatchAPI{coincenterInfo, settings::RunMode::kProd, Duration::max(), true};
  FiatConverter fiatConverter{coincenterInfo, Duration::max()};  // max to avoid real Fiat converter queries
  MockExchangePublic exchangePublic{kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo};
  APIKey key{"test", "testuser", "", "", ""};
  MockExchangePrivate exchangePrivate{exchangePublic, coincenterInfo, key};

  Market market{"ETH", "EUR"};

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

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(bidPrice1);

  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, market, TradeSide::kSell, tradeOptions);

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
  MonetaryAmount pri(*marketOrderBook1.computeAvgPriceForTakerAmount(from));

  MonetaryAmount vol(from / pri, market.base());
  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, market, TradeSide::kBuy, tradeOptions);

  MonetaryAmount tradedTo = vol * pri.toNeutral();

  EXPECT_CALL(exchangePublic, queryOrderBook(market, MarketOrderBook::kDefaultDepth))
      .WillOnce(testing::Return(marketOrderBook1));
  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(
          testing::Return(PlaceOrderInfo(OrderInfo(TradedAmounts(from, tradedTo), true), OrderId("OrderId # 0"))));

  EXPECT_EQ(exchangePrivate.trade(from, market.base(), tradeOptions), TradedAmounts(from, tradedTo));
}

TEST_F(ExchangePrivateTest, MakerTradeBaseToQuote) {
  tradeBaseExpectCalls();

  MonetaryAmount from(10, market.base());
  MonetaryAmount vol(from);
  MonetaryAmount pri(askPrice1);

  TradeSide side = TradeSide::kSell;
  PriceOptions priceOptions(PriceStrategy::kMaker);
  TradeOptions tradeOptions(priceOptions);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, market, side, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  PlaceOrderInfo unmatchedPlacedOrderInfo(OrderInfo(TradedAmounts(from.currencyCode(), market.quote()), false),
                                          OrderId("Order # 0"));

  OrderRef orderRef(unmatchedPlacedOrderInfo.orderId, nbSecondsSinceEpoch, market, side);

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo));

  MonetaryAmount partialMatchedFrom = from / 5;
  MonetaryAmount partialMatchedTo = partialMatchedFrom.toNeutral() * askPrice1;
  MonetaryAmount fullMatchedTo = from.toNeutral() * askPrice1;

  EXPECT_CALL(exchangePrivate, queryOrderInfo(orderRef))
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
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount vol1(from / pri1, market.base());
  MonetaryAmount vol2(from / pri2, market.base());

  TradeOptions tradeOptions(TradeTimeoutAction::kCancel, TradeMode::kReal, Duration::max(), Duration::zero(),
                            TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, market, side, tradeOptions);

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

  OrderRef orderRef1(unmatchedPlacedOrderInfo1.orderId, nbSecondsSinceEpoch, market, side);
  OrderRef orderRef2(unmatchedPlacedOrderInfo2.orderId, nbSecondsSinceEpoch, market, side);

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
  MonetaryAmount partialMatchedTo(partialMatchedFrom / bidPrice2, market.base());

  TradedAmounts partialMatchedTradedAmounts(partialMatchedFrom, partialMatchedTo);

  EXPECT_CALL(exchangePrivate, queryOrderInfo(orderRef2))
      .WillOnce(testing::Return(unmatchedPlacedOrderInfo2.orderInfo))
      .WillOnce(testing::Return(OrderInfo(partialMatchedTradedAmounts, false)));

  EXPECT_CALL(exchangePrivate, cancelOrder(orderRef2))
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

  TradeSide side = TradeSide::kSell;
  TradeOptions tradeOptions(TradeTimeoutAction::kCancel, TradeMode::kSimulation, Duration::max(), Duration::zero(),
                            TradeTypePolicy::kForceMultiTrade);
  TradeInfo tradeInfo(nbSecondsSinceEpoch, market, side, tradeOptions);

  EXPECT_CALL(exchangePublic, queryOrderBook(market, testing::_)).WillOnce(testing::Return(marketOrderBook1));

  EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo)).Times(0);

  // In simulation mode, fee is applied
  MonetaryAmount toAmount =
      exchangePublic.exchangeInfo().applyFee(from.toNeutral() * askPrice1, ExchangeInfo::FeeType::kMaker);

  EXPECT_EQ(exchangePrivate.trade(from, market.quote(), tradeOptions), TradedAmounts(from, toAmount));
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

  MonetaryAmount fee(1, "ETH", 2);
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

class ExchangePrivateDustSweeperTest : public ExchangePrivateTest {
 protected:
  ExchangePrivateDustSweeperTest() {
    EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
  }

  void expectQueryTradableMarkets() {
    EXPECT_CALL(exchangePublic, queryTradableMarkets())
        .WillOnce(testing::Return(MarketSet{xrpbtcMarket, xrpeurMarket, Market{"ETH", "EUR"}}));
  }

  void expectMarketOrderBookCall(Market m, int nTimes = 1) {
    if (m == xrpbtcMarket) {
      EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
          .Times(nTimes)
          .WillRepeatedly(testing::Return(xrpbtcMarketOrderBook));
    } else if (m == xrpeurMarket) {
      EXPECT_CALL(exchangePublic, queryOrderBook(m, MarketOrderBook::kDefaultDepth))
          .Times(nTimes)
          .WillRepeatedly(testing::Return(xrpeurMarketOrderBook));
    } else {
      throw exception("Invalid market");
    }
  }

  TradedAmounts expectTakerSell(MonetaryAmount from, MonetaryAmount pri, int percentageSold = 100) {
    MonetaryAmount vol(from);

    Market m{from.currencyCode(), pri.currencyCode()};
    TradeInfo tradeInfo(nbSecondsSinceEpoch, m, TradeSide::kSell, tradeOptions);

    MonetaryAmount tradedTo = vol.toNeutral() * pri;

    TradedAmounts tradedAmounts;
    if (percentageSold == 100) {
      tradedAmounts = TradedAmounts(from, tradedTo);  // to avoid rounding issues
    } else {
      tradedAmounts = TradedAmounts((from * percentageSold) / 100, (tradedTo * percentageSold) / 100);
    }

    OrderId orderId{"OrderId # "};
    AppendString(orderId, orderIdInt++);

    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, tradeInfo))
        .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(tradedAmounts, true), orderId)));

    return tradedAmounts;
  }

  TradedAmounts expectTakerBuy(MonetaryAmount to, MonetaryAmount askPri, MonetaryAmount bidPri, Market m,
                               bool success = true) {
    MonetaryAmount from = to.toNeutral() * bidPri;
    MonetaryAmount vol(from / askPri, m.base());

    TradeInfo tradeInfo(nbSecondsSinceEpoch, m, TradeSide::kBuy, tradeOptions);

    TradedAmounts tradedAmounts(MonetaryAmount{success ? from : MonetaryAmount(0), askPri.currencyCode()},
                                success ? vol : MonetaryAmount{0, vol.currencyCode()});

    OrderId orderId{"OrderId # "};
    AppendString(orderId, orderIdInt++);

    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, askPri, tradeInfo))
        .WillOnce(testing::Return(PlaceOrderInfo(OrderInfo(tradedAmounts, true), orderId)));

    return tradedAmounts;
  }

  void expectMarketPricesMapCall() {
    EXPECT_CALL(exchangePublic, queryAllPrices()).WillOnce(testing::Return(marketPriceMap));
  }

  std::optional<MonetaryAmount> dustThreshold(CurrencyCode cur) {
    const auto &dustThresholds = exchangePublic.exchangeInfo().dustAmountsThreshold();
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

  PriceOptions priceOptions{PriceStrategy::kTaker};
  TradeOptions tradeOptions{priceOptions};

  MonetaryAmount xrpbtcBidPri{31, "BTC", 6};
  MonetaryAmount xrpbtcAskPri{32, "BTC", 6};
  MarketOrderBook xrpbtcMarketOrderBook{
      xrpbtcAskPri, MonetaryAmount(40, dustCur), xrpbtcBidPri, MonetaryAmount(27909, dustCur, 3), {3, 6}, depth};

  MonetaryAmount xrpeurBidPri{5, "EUR", 1};
  MonetaryAmount xrpeurAskPri{51, "EUR", 2};
  MarketOrderBook xrpeurMarketOrderBook{
      xrpeurAskPri, MonetaryAmount(40, dustCur), xrpeurBidPri, MonetaryAmount(27909, dustCur, 3), {3, 2}, depth};

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
      .WillOnce(testing::Return(BalancePortfolio{avBtcAmount + tradedAmounts.tradedTo}));

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

  MonetaryAmount xrpDustThreshold = *dustThreshold(dustCur);
  TradedAmounts tradedAmounts1 = expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket);

  TradedAmounts tradedAmounts2 = expectTakerSell(from + tradedAmounts1.tradedTo, pri);

  MonetaryAmount avBtcAmount{75, "BTC", 4};
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions))
      .WillOnce(testing::Return(BalancePortfolio{from, avBtcAmount}))
      .WillOnce(
          testing::Return(BalancePortfolio{from + tradedAmounts1.tradedTo, avBtcAmount - tradedAmounts1.tradedFrom}))
      .WillOnce(testing::Return(BalancePortfolio{avBtcAmount - tradedAmounts1.tradedFrom}));

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

  ::testing::InSequence s;

  MonetaryAmount avBtcAmount{75, "BTC", 4};
  MonetaryAmount avEurAmount{500, "EUR"};

  BalancePortfolio balance1{from, avBtcAmount, avEurAmount};

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance1));

  // BTC should be called first because markets are lexicographically sorted
  expectTakerSell(from, priBtc, 0);  // no selling possible
  expectTakerSell(from, priEur, 0);  // no selling possible

  MonetaryAmount xrpDustThreshold = *dustThreshold(dustCur);
  TradedAmounts tradedAmounts1 = expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket);
  from += tradedAmounts1.tradedTo;

  BalancePortfolio balance2 = balance1 + tradedAmounts1;

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance2));

  int percentXRPSoldSecondStep = 80;
  TradedAmounts tradedAmounts2 = expectTakerSell(from, priBtc, percentXRPSoldSecondStep);
  from -= tradedAmounts2.tradedFrom;

  BalancePortfolio balance3 = balance2 + tradedAmounts2;

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance3));

  expectTakerSell(from, priEur, 0);  // no selling possible
  expectTakerSell(from, priBtc, 0);  // no selling possible

  // try buy with multiplier 1x, not possible
  expectTakerBuy(xrpDustThreshold, xrpeurAskPri, xrpeurBidPri, xrpeurMarket, false);
  expectTakerBuy(xrpDustThreshold, xrpbtcAskPri, xrpbtcBidPri, xrpbtcMarket, false);

  TradedAmounts tradedAmounts3 = expectTakerBuy((3 * xrpDustThreshold) / 2, xrpeurAskPri, xrpeurBidPri, xrpeurMarket);
  from += tradedAmounts3.tradedTo;

  BalancePortfolio balance4 = balance3 + tradedAmounts3;
  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance4));

  // Final sell that works
  TradedAmounts tradedAmounts4 = expectTakerSell(from, priEur);

  BalancePortfolio balance5 = balance4 + tradedAmounts4;

  EXPECT_CALL(exchangePrivate, queryAccountBalance(balanceOptions)).WillOnce(testing::Return(balance5));

  TradedAmountsVector tradedAmountsVector{tradedAmounts1, tradedAmounts2, tradedAmounts3, tradedAmounts4};
  TradedAmountsVectorWithFinalAmount res{tradedAmountsVector, MonetaryAmount{0, dustCur}};
  EXPECT_EQ(exchangePrivate.queryDustSweeper(dustCur), res);
}

}  // namespace cct::api