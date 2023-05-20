#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "exchangedata_test.hpp"
#include "exchangesorchestrator.hpp"

namespace cct {

using OrderInfo = api::OrderInfo;
using PlaceOrderInfo = api::PlaceOrderInfo;

class ExchangeOrchestratorTest : public ExchangesBaseTest {
 protected:
  ExchangesOrchestrator exchangesOrchestrator{std::span<Exchange>(&this->exchange1, 8)};
};

/// For the trade tests, 'exchangeprivateapi_test' already tests a lot of complex trade options.
/// Here we are only interested in testing the orchestrator, so we for simplicity reasons we will do only taker trades.

#define EXPECT_TRADE(exchangePublic, exchangePrivate)                                                               \
  if (tradableMarketsCall == TradableMarkets::kExpectCall) {                                                        \
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(markets));                         \
  } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {                                               \
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).Times(0);                                                   \
  }                                                                                                                 \
                                                                                                                    \
  switch (orderBookCall) {                                                                                          \
    case OrderBook::kExpect2Calls:                                                                                  \
    case OrderBook::kExpect3Calls:                                                                                  \
    case OrderBook::kExpect4Calls:                                                                                  \
    case OrderBook::kExpect5Calls:                                                                                  \
      EXPECT_CALL(exchangePublic, queryOrderBook(mk, depth))                                                        \
          .Times(static_cast<int>(orderBookCall))                                                                   \
          .WillRepeatedly(testing::Return(marketOrderbook));                                                        \
      break;                                                                                                        \
    case OrderBook::kExpectCall:                                                                                    \
      EXPECT_CALL(exchangePublic, queryOrderBook(mk, depth)).WillOnce(testing::Return(marketOrderbook));            \
      break;                                                                                                        \
    case OrderBook::kExpectNoCall:                                                                                  \
      EXPECT_CALL(exchangePublic, queryOrderBook(mk, depth)).Times(0);                                              \
      break;                                                                                                        \
    case OrderBook::kNoExpectation:                                                                                 \
      break;                                                                                                        \
  }                                                                                                                 \
                                                                                                                    \
  if (allOrderBooksCall == AllOrderBooks::kExpectCall) {                                                            \
    EXPECT_CALL(exchangePublic, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));   \
  } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {                                                   \
    EXPECT_CALL(exchangePublic, queryAllApproximatedOrderBooks(1)).Times(0);                                        \
  }                                                                                                                 \
                                                                                                                    \
  EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));                 \
  if (makeMarketAvailable && from > 0) {                                                                            \
    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, testing::_)).WillOnce(testing::Return(placeOrderInfo)); \
  } else {                                                                                                          \
    EXPECT_CALL(exchangePrivate, placeOrder(from, vol, pri, testing::_)).Times(0);                                  \
  }

#define EXPECT_TWO_STEP_TRADE(exchangePublic, exchangePrivate, m1, m2)                                                 \
  if (tradableMarketsCall == TradableMarkets::kExpectCall) {                                                           \
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).WillOnce(testing::Return(markets));                            \
  } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {                                                  \
    EXPECT_CALL(exchangePublic, queryTradableMarkets()).Times(0);                                                      \
  }                                                                                                                    \
                                                                                                                       \
  switch (orderBookCall) {                                                                                             \
    case OrderBook::kExpect2Calls:                                                                                     \
    case OrderBook::kExpect3Calls:                                                                                     \
    case OrderBook::kExpect4Calls:                                                                                     \
    case OrderBook::kExpect5Calls:                                                                                     \
      EXPECT_CALL(exchangePublic, queryOrderBook(m1, depth))                                                           \
          .Times(static_cast<int>(orderBookCall))                                                                      \
          .WillRepeatedly(testing::Return(marketOrderbook1));                                                          \
      EXPECT_CALL(exchangePublic, queryOrderBook(m2, depth))                                                           \
          .Times(static_cast<int>(orderBookCall))                                                                      \
          .WillRepeatedly(testing::Return(marketOrderbook2));                                                          \
      break;                                                                                                           \
    case OrderBook::kExpectCall:                                                                                       \
      EXPECT_CALL(exchangePublic, queryOrderBook(m1, depth)).WillOnce(testing::Return(marketOrderbook1));              \
      EXPECT_CALL(exchangePublic, queryOrderBook(m2, depth)).WillOnce(testing::Return(marketOrderbook2));              \
      break;                                                                                                           \
    case OrderBook::kExpectNoCall:                                                                                     \
      EXPECT_CALL(exchangePublic, queryOrderBook(m1, depth)).Times(0);                                                 \
      EXPECT_CALL(exchangePublic, queryOrderBook(m2, depth)).Times(0);                                                 \
      break;                                                                                                           \
    case OrderBook::kNoExpectation:                                                                                    \
      break;                                                                                                           \
  }                                                                                                                    \
                                                                                                                       \
  if (allOrderBooksCall == AllOrderBooks::kExpectCall) {                                                               \
    EXPECT_CALL(exchangePublic, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));      \
  } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {                                                      \
    EXPECT_CALL(exchangePublic, queryAllApproximatedOrderBooks(1)).Times(0);                                           \
  }                                                                                                                    \
                                                                                                                       \
  EXPECT_CALL(exchangePrivate, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));                    \
  if (makeMarketAvailable && from > 0) {                                                                               \
    EXPECT_CALL(exchangePrivate, placeOrder(from, vol2, pri2, testing::_)).WillOnce(testing::Return(placeOrderInfo1)); \
    EXPECT_CALL(exchangePrivate, placeOrder(MonetaryAmount(from, interCur), vol1, pri1, testing::_))                   \
        .WillOnce(testing::Return(placeOrderInfo2));                                                                   \
  } else {                                                                                                             \
    EXPECT_CALL(exchangePrivate, placeOrder(from, vol2, pri2, testing::_)).Times(0);                                   \
    EXPECT_CALL(exchangePrivate, placeOrder(MonetaryAmount(from, interCur), vol1, pri1, testing::_)).Times(0);         \
  }

class ExchangeOrchestratorTradeTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorTradeTest() { resetMarkets(); }

  enum class TradableMarkets : int8_t { kExpectNoCall, kExpectCall, kNoExpectation };
  enum class OrderBook : int8_t {
    kExpectNoCall,
    kExpectCall,
    kExpect2Calls,
    kExpect3Calls,
    kExpect4Calls,
    kExpect5Calls,
    kNoExpectation
  };
  enum class AllOrderBooks : int8_t { kExpectNoCall, kExpectCall, kNoExpectation };

  TradedAmounts expectSingleTrade(int exchangePrivateNum, MonetaryAmount from, CurrencyCode toCurrency, TradeSide side,
                                  TradableMarkets tradableMarketsCall, OrderBook orderBookCall,
                                  AllOrderBooks allOrderBooksCall, bool makeMarketAvailable) {
    Market mk(from.currencyCode(), toCurrency);
    if (side == TradeSide::kBuy) {
      mk = mk.reverse();
    }

    // Choose price of 1 such that we do not need to make a division if it's a buy.
    MonetaryAmount vol(from, mk.base());
    MonetaryAmount pri(1, mk.quote());

    MonetaryAmount maxVol(std::numeric_limits<MonetaryAmount::AmountType>::max(), mk.base(),
                          volAndPriDec1.volNbDecimals);

    MonetaryAmount tradedTo(from, toCurrency);

    MonetaryAmount deltaPri(1, pri.currencyCode(), volAndPriDec1.priNbDecimals);
    MonetaryAmount askPrice = side == TradeSide::kBuy ? pri : pri + deltaPri;
    MonetaryAmount bidPrice = side == TradeSide::kSell ? pri : pri - deltaPri;
    MarketOrderBook marketOrderbook{askPrice, maxVol, bidPrice, maxVol, volAndPriDec1, MarketOrderBook::kDefaultDepth};

    TradedAmounts tradedAmounts(from, tradedTo);
    OrderId orderId("OrderId # 0");
    OrderInfo orderInfo(tradedAmounts, true);
    PlaceOrderInfo placeOrderInfo(orderInfo, orderId);

    if (makeMarketAvailable) {
      markets.insert(mk);
      marketOrderBookMap.insert_or_assign(mk, marketOrderbook);
    }

    // EXPECT_CALL does not allow references. Or I did not found the way to make it work, so we use ugly macros here
    switch (exchangePrivateNum) {
      case 1:
        EXPECT_TRADE(exchangePublic1, exchangePrivate1)
        break;
      case 2:
        EXPECT_TRADE(exchangePublic2, exchangePrivate2)
        break;
      case 3:
        EXPECT_TRADE(exchangePublic3, exchangePrivate3)
        break;
      case 4:
        EXPECT_TRADE(exchangePublic3, exchangePrivate4)
        break;
      case 5:
        EXPECT_TRADE(exchangePublic3, exchangePrivate5)
        break;
      case 6:
        EXPECT_TRADE(exchangePublic3, exchangePrivate6)
        break;
      case 7:
        EXPECT_TRADE(exchangePublic3, exchangePrivate7)
        break;
      case 8:
        EXPECT_TRADE(exchangePublic1, exchangePrivate8)
        break;
      default:
        throw exception("Unexpected exchange number ");
    }

    return tradedAmounts;
  }

  TradedAmounts expectTwoStepTrade(int exchangePrivateNum, MonetaryAmount from, CurrencyCode toCurrency, TradeSide side,
                                   TradableMarkets tradableMarketsCall, OrderBook orderBookCall,
                                   AllOrderBooks allOrderBooksCall, bool makeMarketAvailable) {
    CurrencyCode interCur("AAA");
    Market market1(from.currencyCode(), interCur);
    Market market2(interCur, toCurrency);
    if (side == TradeSide::kBuy) {
      market1 = Market(toCurrency, interCur);
      market2 = Market(interCur, from.currencyCode());
    } else {
      market1 = Market(from.currencyCode(), interCur);
      market2 = Market(interCur, toCurrency);
    }

    // Choose price of 1 such that we do not need to make a division if it's a buy.
    MonetaryAmount vol1(from, market1.base());
    MonetaryAmount vol2(from, market2.base());
    MonetaryAmount pri1(1, market1.quote());
    MonetaryAmount pri2(1, market2.quote());

    MonetaryAmount maxVol1(std::numeric_limits<MonetaryAmount::AmountType>::max(), market1.base(),
                           volAndPriDec1.volNbDecimals);
    MonetaryAmount maxVol2(std::numeric_limits<MonetaryAmount::AmountType>::max(), market2.base(),
                           volAndPriDec1.volNbDecimals);

    MonetaryAmount tradedTo1(from, interCur);
    MonetaryAmount tradedTo2(from, toCurrency);

    MonetaryAmount deltaPri1(1, pri1.currencyCode(), volAndPriDec1.priNbDecimals);
    MonetaryAmount deltaPri2(1, pri2.currencyCode(), volAndPriDec1.priNbDecimals);
    MonetaryAmount askPri1 = side == TradeSide::kBuy ? pri1 : pri1 + deltaPri1;
    MonetaryAmount askPri2 = side == TradeSide::kBuy ? pri2 : pri2 + deltaPri2;
    MonetaryAmount bidPri1 = side == TradeSide::kSell ? pri1 : pri1 - deltaPri1;
    MonetaryAmount bidPri2 = side == TradeSide::kSell ? pri2 : pri2 - deltaPri2;
    MarketOrderBook marketOrderbook1{askPri1, maxVol1, bidPri1, maxVol1, volAndPriDec1, MarketOrderBook::kDefaultDepth};
    MarketOrderBook marketOrderbook2{askPri2, maxVol2, bidPri2, maxVol2, volAndPriDec1, MarketOrderBook::kDefaultDepth};

    TradedAmounts tradedAmounts1(from, vol2);
    TradedAmounts tradedAmounts2(MonetaryAmount(from, interCur), tradedTo2);

    OrderId orderId1("OrderId # 0");
    OrderId orderId2("OrderId # 1");
    OrderInfo orderInfo1(tradedAmounts1, true);
    OrderInfo orderInfo2(tradedAmounts2, true);
    PlaceOrderInfo placeOrderInfo1(orderInfo1, orderId1);
    PlaceOrderInfo placeOrderInfo2(orderInfo2, orderId2);

    if (makeMarketAvailable) {
      markets.insert(market1);
      markets.insert(market2);

      marketOrderBookMap.insert_or_assign(market1, marketOrderbook1);
      marketOrderBookMap.insert_or_assign(market2, marketOrderbook2);
    }

    // EXPECT_CALL does not allow references. Or I did not found the way to make it work, so we use ugly macros here
    switch (exchangePrivateNum) {
      case 1:
        EXPECT_TWO_STEP_TRADE(exchangePublic1, exchangePrivate1, market1, market2)
        break;
      case 2:
        EXPECT_TWO_STEP_TRADE(exchangePublic2, exchangePrivate2, market1, market2)
        break;
      case 3:
        EXPECT_TWO_STEP_TRADE(exchangePublic3, exchangePrivate3, market1, market2)
        break;
      case 4:
        EXPECT_TWO_STEP_TRADE(exchangePublic3, exchangePrivate4, market1, market2)
        break;
      case 5:
        EXPECT_TWO_STEP_TRADE(exchangePublic3, exchangePrivate5, market1, market2)
        break;
      case 6:
        EXPECT_TWO_STEP_TRADE(exchangePublic3, exchangePrivate6, market1, market2)
        break;
      case 7:
        EXPECT_TWO_STEP_TRADE(exchangePublic3, exchangePrivate7, market1, market2)
        break;
      case 8:
        EXPECT_TWO_STEP_TRADE(exchangePublic1, exchangePrivate8, market1, market2)
        break;
      default:
        throw exception("Unexpected exchange number ");
    }

    return {from, tradedTo2};
  }

  void resetMarkets() {
    marketOrderBookMap.clear();
    markets = {Market("DU1", "DU2"), Market("DU3", "DU2"), Market("DU4", "DU5")};
  }

  PriceOptions priceOptions{PriceStrategy::kTaker};
  TradeOptions tradeOptions{priceOptions,     TradeTimeoutAction::kCancel, TradeMode::kReal, Duration::max(),
                            Duration::zero(), TradeTypePolicy::kDefault};
  bool isPercentageTrade = false;
  MarketOrderBookMap marketOrderBookMap;
  MarketSet markets;
};

TEST_F(ExchangeOrchestratorTradeTest, SingleExchangeBuy) {
  MonetaryAmount from(100, "EUR");
  CurrencyCode toCurrency("XRP");
  TradeSide side = TradeSide::kBuy;
  TradedAmounts tradedAmounts = expectSingleTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                                  OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName())};

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            TradedAmountsPerExchange(1, std::make_pair(&exchange1, tradedAmounts)));
}

TEST_F(ExchangeOrchestratorTradeTest, NoAvailableAmountToSell) {
  MonetaryAmount from(10, "SOL");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(BalanceOptions())).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(BalanceOptions())).WillOnce(testing::Return(balancePortfolio2));

  MonetaryAmount zero(0, from.currencyCode());
  expectSingleTrade(2, zero, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, false);
  expectSingleTrade(1, zero, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, true);

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            TradedAmountsPerExchange{});
}

TEST_F(ExchangeOrchestratorTradeTest, TwoAccountsSameExchangeSell) {
  MonetaryAmount from(2, "ETH");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange4.name(), exchange4.keyName())};

  // 1.5ETH
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(BalanceOptions())).WillOnce(testing::Return(balancePortfolio1));

  // 0.6ETH
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(BalanceOptions())).WillOnce(testing::Return(balancePortfolio3));

  MonetaryAmount traded1("1.5 ETH");
  MonetaryAmount traded2("0.5 ETH");
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, traded1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectSingleTrade(4, traded2, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange3, tradedAmounts3),
                                                    std::make_pair(&exchange4, tradedAmounts4)};
  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, ThreeExchangesBuy) {
  CurrencyCode fromCurrency("USDT");
  MonetaryAmount from(13015, fromCurrency);
  CurrencyCode toCurrency("LUNA");
  TradeSide side = TradeSide::kBuy;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  MonetaryAmount from1(5000, fromCurrency);
  MonetaryAmount from2(6750, fromCurrency);
  MonetaryAmount from3(1265, fromCurrency);

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange2, tradedAmounts2),
                                                    std::make_pair(&exchange1, tradedAmounts1),
                                                    std::make_pair(&exchange3, tradedAmounts3)};
  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, ExchangeNames{}, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, ThreeExchangesBuyNotEnoughAmount) {
  CurrencyCode fromCurrency("USDT");
  MonetaryAmount from(13015, fromCurrency);
  CurrencyCode toCurrency("LUNA");
  TradeSide side = TradeSide::kBuy;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  MonetaryAmount from1(0, fromCurrency);
  MonetaryAmount from2(6750, fromCurrency);
  MonetaryAmount from3(4250, fromCurrency);
  MonetaryAmount from4("107.5", fromCurrency);
  expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectSingleTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange2, tradedAmounts2),
                                                    std::make_pair(&exchange3, tradedAmounts3),
                                                    std::make_pair(&exchange4, tradedAmounts4)};

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, ExchangeNames{}, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, ManyAccountsTrade) {
  CurrencyCode fromCurrency("USDT");
  MonetaryAmount from(40000, fromCurrency);
  CurrencyCode toCurrency("LUNA");
  TradeSide side = TradeSide::kBuy;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  EXPECT_CALL(exchangePrivate5, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate6, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate7, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate8, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  MonetaryAmount from1(5000, fromCurrency);
  MonetaryAmount from2(6750, fromCurrency);
  MonetaryAmount from3(4250, fromCurrency);
  MonetaryAmount from4("107.5", fromCurrency);
  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect5Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectSingleTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  TradedAmounts tradedAmounts5 = expectSingleTrade(5, from1, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts6 = expectSingleTrade(6, from1, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts7 = expectSingleTrade(7, from1, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts8 = expectSingleTrade(8, from1, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmountsPerExchange tradedAmountsPerExchange{
      std::make_pair(&exchange2, tradedAmounts2), std::make_pair(&exchange1, tradedAmounts1),
      std::make_pair(&exchange8, tradedAmounts8), std::make_pair(&exchange5, tradedAmounts5),
      std::make_pair(&exchange6, tradedAmounts6), std::make_pair(&exchange7, tradedAmounts7),
      std::make_pair(&exchange3, tradedAmounts3), std::make_pair(&exchange4, tradedAmounts4)};

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, ExchangeNames{}, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, SingleExchangeBuyAll) {
  CurrencyCode fromCurrency("EUR");
  CurrencyCode toCurrency("XRP");
  TradeSide side = TradeSide::kBuy;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName())};

  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  TradedAmounts tradedAmounts3 =
      expectSingleTrade(3, MonetaryAmount(1500, fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                        OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  constexpr bool kIsPercentageTrade = true;
  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange3, tradedAmounts3)};
  EXPECT_EQ(exchangesOrchestrator.trade(MonetaryAmount(100, fromCurrency), kIsPercentageTrade, toCurrency,
                                        privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSellAll) {
  CurrencyCode fromCurrency("ETH");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName()),
                                               ExchangeName(exchange3.name(), exchange3.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  TradedAmounts tradedAmounts1 =
      expectSingleTrade(1, balancePortfolio1.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                        OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 =
      expectSingleTrade(3, balancePortfolio3.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                        OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  constexpr bool kIsPercentageTrade = true;
  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1),
                                                    std::make_pair(&exchange3, tradedAmounts3)};
  EXPECT_EQ(exchangesOrchestrator.trade(MonetaryAmount(100, fromCurrency), kIsPercentageTrade, toCurrency,
                                        privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, AllExchangesBuyAllOneMarketUnavailable) {
  CurrencyCode fromCurrency("USDT");
  CurrencyCode toCurrency("DOT");
  TradeSide side = TradeSide::kBuy;

  const ExchangeName privateExchangeNames[] = {
      ExchangeName(exchange1.name(), exchange1.keyName()), ExchangeName(exchange3.name(), exchange3.keyName()),
      ExchangeName(exchange2.name(), exchange2.keyName()), ExchangeName(exchange4.name(), exchange4.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  expectSingleTrade(1, MonetaryAmount(0, fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                    OrderBook::kExpectNoCall, AllOrderBooks::kExpectNoCall, false);

  TradedAmounts tradedAmounts2 =
      expectSingleTrade(2, balancePortfolio2.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                        OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 =
      expectSingleTrade(3, balancePortfolio3.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                        OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 =
      expectSingleTrade(4, balancePortfolio4.get(fromCurrency), toCurrency, side, TradableMarkets::kNoExpectation,
                        OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  constexpr bool kIsPercentageTrade = true;
  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange2, tradedAmounts2),
                                                    std::make_pair(&exchange3, tradedAmounts3),
                                                    std::make_pair(&exchange4, tradedAmounts4)};
  EXPECT_EQ(exchangesOrchestrator.trade(MonetaryAmount(100, fromCurrency), kIsPercentageTrade, toCurrency,
                                        privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, SingleExchangeSmartBuy) {
  // Fee is automatically applied on buy
  MonetaryAmount endAmount = MonetaryAmount(1000, "XRP") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from = MonetaryAmount(1000, "USDT");

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1)};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, SingleExchangeSmartBuyTwoSteps) {
  // Fee is automatically applied on buy
  MonetaryAmount endAmount = MonetaryAmount(1000, "XRP") * exchangePublic1.exchangeInfo().getTakerFeeRatio() *
                             exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from = MonetaryAmount(1000, "USDT");

  TradedAmounts tradedAmounts1 = expectTwoStepTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                                    OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1)};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSmartBuy) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from31 = MonetaryAmount(4250, "USDT");
  MonetaryAmount from32 = MonetaryAmount(750, "EUR");

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts31 = expectSingleTrade(3, from31, toCurrency, side, TradableMarkets::kNoExpectation,
                                                    OrderBook::kExpectCall, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts32 = expectSingleTrade(3, from32, toCurrency, side, TradableMarkets::kExpectCall,
                                                    OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1),
                                                    std::make_pair(&exchange3, tradedAmounts31),
                                                    std::make_pair(&exchange3, tradedAmounts32)};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSmartBuyNoMarketOnOneExchange) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(0, "USDT");
  MonetaryAmount from3 = MonetaryAmount(4250, "USDT");

  expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange3, tradedAmounts3)};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, ThreeExchangesSmartBuy) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from2 = MonetaryAmount(0, "USDT");
  MonetaryAmount from41 = MonetaryAmount(0, "USDT");
  MonetaryAmount from42 = MonetaryAmount(1200, "EUR");

  expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, false);

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  resetMarkets();

  expectSingleTrade(4, from41, toCurrency, side, TradableMarkets::kNoExpectation, OrderBook::kExpectNoCall,
                    AllOrderBooks::kNoExpectation, false);

  TradedAmounts tradedAmounts4 = expectSingleTrade(4, from42, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange4.name(), exchange4.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1),
                                                    std::make_pair(&exchange4, tradedAmounts4)};
  EXPECT_EQ(tradedAmountsPerExchange, exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions));
}

TEST_F(ExchangeOrchestratorTradeTest, SmartBuyAllExchanges) {
  CurrencyCode toCurrency("XLM");
  MonetaryAmount endAmount = MonetaryAmount(18800, toCurrency) * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from2 = MonetaryAmount(6750, "USDT");
  MonetaryAmount from31 = MonetaryAmount(1500, "EUR");
  MonetaryAmount from32 = MonetaryAmount(4250, "USDT");
  MonetaryAmount from41 = MonetaryAmount(100, "USDT");
  MonetaryAmount from42 = MonetaryAmount(1200, "EUR");

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts31 = expectSingleTrade(3, from31, toCurrency, side, TradableMarkets::kExpectCall,
                                                    OrderBook::kExpect2Calls, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts32 = expectSingleTrade(3, from32, toCurrency, side, TradableMarkets::kNoExpectation,
                                                    OrderBook::kExpect2Calls, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts41 = expectSingleTrade(4, from41, toCurrency, side, TradableMarkets::kNoExpectation,
                                                    OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts42 = expectSingleTrade(4, from42, toCurrency, side, TradableMarkets::kNoExpectation,
                                                    OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  TradedAmountsPerExchange tradedAmountsPerExchange{
      std::make_pair(&exchange2, tradedAmounts2),  std::make_pair(&exchange1, tradedAmounts1),
      std::make_pair(&exchange3, tradedAmounts32), std::make_pair(&exchange3, tradedAmounts31),
      std::make_pair(&exchange4, tradedAmounts42), std::make_pair(&exchange4, tradedAmounts41)};
  EXPECT_EQ(tradedAmountsPerExchange, exchangesOrchestrator.smartBuy(endAmount, ExchangeNames{}, tradeOptions));
}

TEST_F(ExchangeOrchestratorTradeTest, SingleExchangeSmartSell) {
  MonetaryAmount startAmount = MonetaryAmount(2, "ETH");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from = MonetaryAmount("1.5ETH");

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1)};
  EXPECT_EQ(exchangesOrchestrator.smartSell(startAmount, false, privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, SmartSellAllNoAvailableAmount) {
  MonetaryAmount startAmount = MonetaryAmount(100, "FIL");

  EXPECT_CALL(exchangePublic1, queryTradableMarkets()).Times(0);
  EXPECT_CALL(exchangePublic2, queryTradableMarkets()).Times(0);
  EXPECT_CALL(exchangePublic3, queryTradableMarkets()).Times(0);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  EXPECT_TRUE(exchangesOrchestrator.smartSell(startAmount, true, ExchangeNames{}, tradeOptions).empty());
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSmartSell) {
  MonetaryAmount startAmount = MonetaryAmount(16, "BTC");
  CurrencyCode fromCurrency = startAmount.currencyCode();
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(15, fromCurrency);
  MonetaryAmount from2 = MonetaryAmount("0.5", fromCurrency);

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1),
                                                    std::make_pair(&exchange2, tradedAmounts2)};
  EXPECT_EQ(tradedAmountsPerExchange,
            exchangesOrchestrator.smartSell(startAmount, false, privateExchangeNames, tradeOptions));
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSmartSellPercentage) {
  MonetaryAmount startAmount = MonetaryAmount(25, "ETH");
  CurrencyCode fromCurrency = startAmount.currencyCode();
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount("0.525", fromCurrency);
  MonetaryAmount from3 = MonetaryAmount(0, fromCurrency);

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange3.name(), exchange3.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1)};
  EXPECT_EQ(tradedAmountsPerExchange,
            exchangesOrchestrator.smartSell(startAmount, true, privateExchangeNames, tradeOptions));
}

TEST_F(ExchangeOrchestratorTradeTest, TwoExchangesSmartSellNoMarketOnOneExchange) {
  MonetaryAmount startAmount = MonetaryAmount(10000, "SHIB");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from2 = startAmount;
  MonetaryAmount from3 = MonetaryAmount(0, startAmount.currencyCode());

  expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts2 = expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange2.name(), exchange2.keyName()),
                                               ExchangeName(exchange3.name(), exchange3.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange2, tradedAmounts2)};
  EXPECT_EQ(exchangesOrchestrator.smartSell(startAmount, false, privateExchangeNames, tradeOptions),
            tradedAmountsPerExchange);
}

TEST_F(ExchangeOrchestratorTradeTest, ThreeExchangesSmartSellFromAnotherPreferredCurrency) {
  MonetaryAmount startAmount = MonetaryAmount(2000, "EUR");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from3 = MonetaryAmount(1500, startAmount.currencyCode());
  MonetaryAmount from4 = MonetaryAmount(500, startAmount.currencyCode());

  expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectSingleTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation,
                                                   OrderBook::kNoExpectation, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange4.name(), exchange4.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange3.name(), exchange3.keyName())};

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange3, tradedAmounts3),
                                                    std::make_pair(&exchange4, tradedAmounts4)};
  EXPECT_EQ(tradedAmountsPerExchange,
            exchangesOrchestrator.smartSell(startAmount, false, privateExchangeNames, tradeOptions));
}

TEST_F(ExchangeOrchestratorTradeTest, SmartSellAllExchanges) {
  MonetaryAmount startAmount = MonetaryAmount(1, "ETH");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(1, startAmount.currencyCode());
  MonetaryAmount from2 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from3 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from4 = MonetaryAmount(0, startAmount.currencyCode());

  TradedAmounts tradedAmounts1 = expectSingleTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  expectSingleTrade(2, from2, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, true);
  expectSingleTrade(3, from3, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
                    AllOrderBooks::kExpectNoCall, true);
  expectSingleTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation, OrderBook::kNoExpectation,
                    AllOrderBooks::kNoExpectation, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  TradedAmountsPerExchange tradedAmountsPerExchange{std::make_pair(&exchange1, tradedAmounts1)};
  EXPECT_EQ(tradedAmountsPerExchange,
            exchangesOrchestrator.smartSell(startAmount, false, ExchangeNames{}, tradeOptions));
}
}  // namespace cct
