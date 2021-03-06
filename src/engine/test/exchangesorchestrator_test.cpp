#include "exchangesorchestrator.hpp"

#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "exchangedata_test.hpp"

namespace cct {

using Deposit = CurrencyExchange::Deposit;
using Withdraw = CurrencyExchange::Withdraw;
using Type = CurrencyExchange::Type;
using TradeInfo = api::TradeInfo;
using OrderInfo = api::OrderInfo;
using PlaceOrderInfo = api::PlaceOrderInfo;
using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

class ExchangeOrchestratorTest : public ExchangesBaseTest {
 protected:
  ExchangesOrchestrator exchangesOrchestrator{std::span<Exchange>(&this->exchange1, 8)};
};

TEST_F(ExchangeOrchestratorTest, TickerInformation) {
  const MarketOrderBookMap marketOrderbookMap1 = {{m1, marketOrderBook10}, {m2, marketOrderBook20}};
  EXPECT_CALL(exchangePublic1, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap1));

  const MarketOrderBookMap marketOrderbookMap2 = {{m1, marketOrderBook10}, {m3, marketOrderBook3}};
  EXPECT_CALL(exchangePublic2, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap2));

  const ExchangeName kTestedExchanges12[] = {ExchangeName(kSupportedExchanges[0]),
                                             ExchangeName(kSupportedExchanges[1])};

  ExchangeTickerMaps expectedTickerMaps = {{&exchange1, marketOrderbookMap1}, {&exchange2, marketOrderbookMap2}};
  EXPECT_EQ(exchangesOrchestrator.getTickerInformation(kTestedExchanges12), expectedTickerMaps);
}

class ExchangeOrchestratorMarketOrderbookTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorMarketOrderbookTest() {
    EXPECT_CALL(exchangePublic1, queryTradableMarkets()).WillOnce(testing::Return(markets1));
    EXPECT_CALL(exchangePublic2, queryTradableMarkets()).WillOnce(testing::Return(markets2));
    EXPECT_CALL(exchangePublic3, queryTradableMarkets()).WillOnce(testing::Return(markets3));

    EXPECT_CALL(exchangePublic1, queryOrderBook(testedMarket, testing::_)).WillOnce(testing::Return(marketOrderBook20));
    EXPECT_CALL(exchangePublic3, queryOrderBook(testedMarket, testing::_)).WillOnce(testing::Return(marketOrderBook21));
  }

  Market testedMarket = m2;

  // Mock tradable markets
  const MarketSet markets1{m1, testedMarket};
  const MarketSet markets2{m1, m3};
  const MarketSet markets3{m1, testedMarket, m3};
  CurrencyCode equiCurrencyCode;
  std::optional<int> optDepth;
  MarketOrderBookConversionRates marketOrderBookConversionRates{{exchange1.name(), marketOrderBook20, std::nullopt},
                                                                {exchange3.name(), marketOrderBook21, std::nullopt}};
};

TEST_F(ExchangeOrchestratorMarketOrderbookTest, AllSpecifiedExchanges) {
  const ExchangeName kTestedExchanges123[] = {
      ExchangeName(kSupportedExchanges[0]), ExchangeName(kSupportedExchanges[1]), ExchangeName(kSupportedExchanges[2])};

  EXPECT_EQ(exchangesOrchestrator.getMarketOrderBooks(testedMarket, kTestedExchanges123, equiCurrencyCode, optDepth),
            marketOrderBookConversionRates);
}

TEST_F(ExchangeOrchestratorMarketOrderbookTest, ImplicitAllExchanges) {
  EXPECT_EQ(exchangesOrchestrator.getMarketOrderBooks(testedMarket, ExchangeNameSpan{}, equiCurrencyCode, optDepth),
            marketOrderBookConversionRates);
}

class ExchangeOrchestratorEmptyMarketOrderbookTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorEmptyMarketOrderbookTest() {
    EXPECT_CALL(exchangePublic2, queryTradableMarkets()).WillOnce(testing::Return(markets2));
  }

  Market testedMarket = m2;

  // Mock tradable markets
  const MarketSet markets1{m1, testedMarket};
  const MarketSet markets2{m1, m3};
  const MarketSet markets3{m1, testedMarket, m3};
  CurrencyCode equiCurrencyCode;
  std::optional<int> optDepth;
  MarketOrderBookConversionRates marketOrderBookConversionRates;
};

TEST_F(ExchangeOrchestratorEmptyMarketOrderbookTest, MarketDoesNotExist) {
  const ExchangeName kTestedExchanges2[] = {ExchangeName(kSupportedExchanges[1])};
  EXPECT_EQ(exchangesOrchestrator.getMarketOrderBooks(testedMarket, kTestedExchanges2, equiCurrencyCode, optDepth),
            marketOrderBookConversionRates);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencyUniqueExchange) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));

  const ExchangeName privateExchangeNames[1] = {ExchangeName(exchange1.name(), exchange1.keyName())};
  BalancePerExchange ret{{&exchange1, balancePortfolio1}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencySeveralExchanges) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio3));

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange1.name(), exchange1.keyName()),
                                               ExchangeName(exchange4.name(), exchange4.keyName())};
  BalancePerExchange ret{
      {&exchange1, balancePortfolio1}, {&exchange3, balancePortfolio2}, {&exchange4, balancePortfolio3}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoUniqueExchanges) {
  CurrencyCode depositCurrency{"ETH"};

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange2.name(), exchange2.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("XRP", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  Wallet wallet2{privateExchangeNames[0], depositCurrency, "address1", "", WalletCheck()};
  EXPECT_CALL(exchangePrivate2, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet2));

  WalletPerExchange ret{{&exchange2, wallet2}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoSeveralExchangesWithUnavailableDeposits) {
  CurrencyCode depositCurrency{"XRP"};

  const ExchangeName privateExchangeNames[] = {
      ExchangeName(exchange3.name(), exchange3.keyName()), ExchangeName(exchange1.name(), exchange1.keyName()),
      ExchangeName(exchange2.name(), exchange2.keyName()), ExchangeName(exchange4.name(), exchange4.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies2{
      CurrencyExchangeVector{CurrencyExchange("XLM", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", Deposit::kUnavailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SOL", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange(depositCurrency, Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("EUR", Deposit::kAvailable, Withdraw::kAvailable, Type::kFiat),
  }};
  EXPECT_CALL(exchangePrivate3, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));
  EXPECT_CALL(exchangePrivate4, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies3));

  Wallet wallet31{privateExchangeNames[2], depositCurrency, "address2", "tag2", WalletCheck()};
  EXPECT_CALL(exchangePrivate3, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet31));

  Wallet wallet32{privateExchangeNames[3], depositCurrency, "address3", "tag3", WalletCheck()};
  EXPECT_CALL(exchangePrivate4, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet32));

  WalletPerExchange ret{{&exchange3, wallet31}, {&exchange4, wallet32}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, GetOpenedOrders) {
  OrdersConstraints noConstraints;

  const ExchangeName privateExchangeNames[] = {ExchangeName(exchange3.name(), exchange3.keyName()),
                                               ExchangeName(exchange2.name(), exchange2.keyName()),
                                               ExchangeName(exchange4.name(), exchange4.keyName())};

  Orders orders2{Order("Id1", MonetaryAmount("0.1ETH"), MonetaryAmount("0.9ETH"), MonetaryAmount("0.14BTC"),
                       Clock::now(), TradeSide::kBuy),
                 Order("Id2", MonetaryAmount("15XLM"), MonetaryAmount("76XLM"), MonetaryAmount("0.5EUR"), Clock::now(),
                       TradeSide::kSell)};
  EXPECT_CALL(exchangePrivate2, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders2));

  Orders orders3{};
  EXPECT_CALL(exchangePrivate3, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders3));

  Orders orders4{Order("Id37", MonetaryAmount("0.7ETH"), MonetaryAmount("0.9ETH"), MonetaryAmount("0.14BTC"),
                       Clock::now(), TradeSide::kSell),
                 Order("Id2", MonetaryAmount("15XLM"), MonetaryAmount("19XLM"), MonetaryAmount("0.5EUR"), Clock::now(),
                       TradeSide::kBuy)};
  EXPECT_CALL(exchangePrivate4, queryOpenedOrders(noConstraints)).WillOnce(testing::Return(orders4));

  OpenedOrdersPerExchange ret{{&exchange2, OrdersSet(orders2.begin(), orders2.end())},
                              {&exchange3, OrdersSet(orders3.begin(), orders3.end())},
                              {&exchange4, OrdersSet(orders4.begin(), orders4.end())}};
  EXPECT_EQ(exchangesOrchestrator.getOpenedOrders(privateExchangeNames, noConstraints), ret);
}

TEST_F(ExchangeOrchestratorTest, GetMarketsPerExchangeOneCurrency) {
  CurrencyCode cur1{"LUNA"};
  CurrencyCode cur2{};
  ExchangeNameSpan exchangeNameSpan{};

  Market m4{"LUNA", "BTC"};
  Market m5{"SHIB", "LUNA"};
  Market m6{"DOGE", "EUR"};

  MarketSet markets1{m1, m2, m4, m6};
  EXPECT_CALL(exchangePublic1, queryTradableMarkets()).WillOnce(testing::Return(markets1));
  MarketSet markets2{m1, m2, m3, m4, m5, m6};
  EXPECT_CALL(exchangePublic2, queryTradableMarkets()).WillOnce(testing::Return(markets2));
  MarketSet markets3{m1, m2, m6};
  EXPECT_CALL(exchangePublic3, queryTradableMarkets()).WillOnce(testing::Return(markets3));

  MarketsPerExchange ret{{&exchange1, MarketSet{m4}}, {&exchange2, MarketSet{m4, m5}}, {&exchange3, MarketSet{}}};
  EXPECT_EQ(exchangesOrchestrator.getMarketsPerExchange(cur1, cur2, exchangeNameSpan), ret);
}

TEST_F(ExchangeOrchestratorTest, GetMarketsPerExchangeTwoCurrencies) {
  CurrencyCode cur1{"LUNA"};
  CurrencyCode cur2{"SHIB"};
  ExchangeNameSpan exchangeNameSpan{};

  Market m4{"LUNA", "BTC"};
  Market m5{"SHIB", "LUNA"};
  Market m6{"DOGE", "EUR"};
  Market m7{"LUNA", "EUR"};

  MarketSet markets1{m1, m2, m4, m6, m7};
  EXPECT_CALL(exchangePublic1, queryTradableMarkets()).WillOnce(testing::Return(markets1));
  MarketSet markets2{m1, m2, m3, m4, m5, m6};
  EXPECT_CALL(exchangePublic2, queryTradableMarkets()).WillOnce(testing::Return(markets2));
  MarketSet markets3{m1, m2, m6, m7};
  EXPECT_CALL(exchangePublic3, queryTradableMarkets()).WillOnce(testing::Return(markets3));

  MarketsPerExchange ret{{&exchange1, MarketSet{}}, {&exchange2, MarketSet{m5}}, {&exchange3, MarketSet{}}};
  EXPECT_EQ(exchangesOrchestrator.getMarketsPerExchange(cur1, cur2, exchangeNameSpan), ret);
}

TEST_F(ExchangeOrchestratorTest, GetExchangesTradingCurrency) {
  CurrencyCode currencyCode{"XRP"};

  const ExchangeName kTestedExchanges13[] = {ExchangeName(kSupportedExchanges[0]),
                                             ExchangeName(kSupportedExchanges[2])};

  CurrencyExchangeFlatSet tradableCurrencies1{
      CurrencyExchangeVector{CurrencyExchange("XRP", Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies())
      .Times(2)
      .WillRepeatedly(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", Deposit::kUnavailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SOL", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("XRP", Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("EUR", Deposit::kAvailable, Withdraw::kAvailable, Type::kFiat),
  }};
  EXPECT_CALL(exchangePrivate3, queryTradableCurrencies())
      .Times(2)
      .WillRepeatedly(testing::Return(tradableCurrencies3));

  UniquePublicSelectedExchanges ret1{&exchange1, &exchange3};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingCurrency(currencyCode, kTestedExchanges13, false), ret1);
  UniquePublicSelectedExchanges ret2{&exchange1};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingCurrency(currencyCode, kTestedExchanges13, true), ret2);
}

TEST_F(ExchangeOrchestratorTest, GetExchangesTradingMarket) {
  constexpr int kNbTests = 5;
  const MarketSet markets1{Market("SOL", "BTC"),   Market("XRP", "BTC"),   Market("XRP", "EUR"),
                           Market("SHIB", "DOGE"), Market("SHIB", "USDT"), Market("XLM", "BTC")};
  EXPECT_CALL(exchangePublic1, queryTradableMarkets()).Times(kNbTests).WillRepeatedly(testing::Return(markets1));

  const MarketSet markets2{Market("SOL", "BTC"), Market("XRP", "BTC"), Market("XRP", "KRW"), Market("SHIB", "KRW"),
                           Market("XLM", "KRW")};
  EXPECT_CALL(exchangePublic2, queryTradableMarkets()).Times(kNbTests).WillRepeatedly(testing::Return(markets2));

  const MarketSet markets3{Market("LUNA", "BTC"), Market("AVAX", "USD"), Market("SOL", "BTC"), Market("XRP", "BTC"),
                           Market("XRP", "KRW"),  Market("SHIB", "KRW"), Market("XLM", "BTC")};
  EXPECT_CALL(exchangePublic3, queryTradableMarkets()).Times(kNbTests).WillRepeatedly(testing::Return(markets3));

  UniquePublicSelectedExchanges ret1{&exchange1, &exchange2, &exchange3};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingMarket(Market("SOL", "BTC"), ExchangeNameSpan{}), ret1);
  UniquePublicSelectedExchanges ret2{&exchange3};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingMarket(Market("AVAX", "USD"), ExchangeNameSpan{}), ret2);
  UniquePublicSelectedExchanges ret3{};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingMarket(Market("SHIB", "EUR"), ExchangeNameSpan{}), ret3);
  UniquePublicSelectedExchanges ret4{};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingMarket(Market("BTC", "SOL"), ExchangeNameSpan{}), ret4);
  UniquePublicSelectedExchanges ret5{&exchange1, &exchange3};
  EXPECT_EQ(exchangesOrchestrator.getExchangesTradingMarket(Market("XLM", "BTC"), ExchangeNameSpan{}), ret5);
}

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
      EXPECT_CALL(exchangePublic, queryOrderBook(m, depth))                                                         \
          .Times(static_cast<int>(orderBookCall))                                                                   \
          .WillRepeatedly(testing::Return(marketOrderbook));                                                        \
      break;                                                                                                        \
    case OrderBook::kExpectCall:                                                                                    \
      EXPECT_CALL(exchangePublic, queryOrderBook(m, depth)).WillOnce(testing::Return(marketOrderbook));             \
      break;                                                                                                        \
    case OrderBook::kExpectNoCall:                                                                                  \
      EXPECT_CALL(exchangePublic, queryOrderBook(m, depth)).Times(0);                                               \
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
  if (makeMarketAvailable && from.isStrictlyPositive()) {                                                           \
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
  if (makeMarketAvailable && from.isStrictlyPositive()) {                                                              \
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
    Market m(from.currencyCode(), toCurrency);
    if (side == TradeSide::kBuy) {
      m = m.reverse();
    }

    // Choose price of 1 such that we do not need to make a division if it's a buy.
    MonetaryAmount vol(from, m.base());
    MonetaryAmount pri(1, m.quote());

    MonetaryAmount maxVol(std::numeric_limits<MonetaryAmount::AmountType>::max(), m.base(),
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
      markets.insert(m);
      marketOrderBookMap.insert_or_assign(m, marketOrderbook);
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

    return TradedAmounts(from, tradedTo2);
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

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));

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

  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  MonetaryAmount ratio3("0.75");
  MonetaryAmount ratio4 = MonetaryAmount(1) - ratio3;
  TradedAmounts tradedAmounts3 = expectSingleTrade(3, from * ratio3, toCurrency, side, TradableMarkets::kExpectCall,
                                                   OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectSingleTrade(4, from * ratio4, toCurrency, side, TradableMarkets::kNoExpectation,
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

TEST_F(ExchangeOrchestratorTest, WithdrawSameAccountImpossible) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  const ExchangeName &toExchange = fromExchange;
  EXPECT_THROW(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange), exception);
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleFrom) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  ExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange).hasBeenInitiated());
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleTo) {
  MonetaryAmount grossAmount{1000, "XRP"};
  ExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  ExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, false, fromExchange, toExchange).hasBeenInitiated());
}

inline bool operator==(const WithdrawInfo &lhs, const WithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

namespace api {
inline bool operator==(const InitiatedWithdrawInfo &lhs, const InitiatedWithdrawInfo &rhs) {
  return lhs.withdrawId() == rhs.withdrawId();
}

inline bool operator==(const SentWithdrawInfo &lhs, const SentWithdrawInfo &rhs) {
  return lhs.isWithdrawSent() == rhs.isWithdrawSent() && lhs.netEmittedAmount() == rhs.netEmittedAmount();
}
}  // namespace api

class ExchangeOrchestratorWithdrawTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorWithdrawTest() {
    CurrencyExchangeFlatSet tradableCurrencies1{
        CurrencyExchangeVector{CurrencyExchange(cur, Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
                               CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
    CurrencyExchangeFlatSet tradableCurrencies2{
        CurrencyExchangeVector{CurrencyExchange(cur, Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
                               CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};

    EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
    EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));
  }

  WithdrawInfo createWithdrawInfo(MonetaryAmount grossAmount, bool isPercentageWithdraw) {
    if (isPercentageWithdraw) {
      EXPECT_CALL(exchangePrivate1, queryAccountBalance(CurrencyCode())).WillOnce(testing::Return(balancePortfolio1));
      grossAmount = (grossAmount.toNeutral() * balancePortfolio1.get(cur)) / 100;
    } else {
      EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).Times(0);
    }
    MonetaryAmount netEmittedAmount = grossAmount - fee;
    Wallet receivingWallet{toExchange, cur, address, tag, WalletCheck()};
    EXPECT_CALL(exchangePrivate2, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

    api::InitiatedWithdrawInfo initiatedWithdrawInfo{receivingWallet, withdrawIdView, grossAmount};
    EXPECT_CALL(exchangePrivate1, launchWithdraw(grossAmount, std::move(receivingWallet)))
        .WillOnce(testing::Return(initiatedWithdrawInfo));
    api::SentWithdrawInfo sentWithdrawInfo{netEmittedAmount, true};
    EXPECT_CALL(exchangePrivate1, isWithdrawSuccessfullySent(initiatedWithdrawInfo))
        .WillOnce(testing::Return(sentWithdrawInfo));
    EXPECT_CALL(exchangePrivate2, isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo))
        .WillOnce(testing::Return(true));
    return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
  }

  CurrencyCode cur{"XRP"};
  ExchangeName fromExchange{exchange1.name(), exchange1.keyName()};
  ExchangeName toExchange{exchange2.name(), exchange2.keyName()};

  std::string_view address{"TestAddress"};
  std::string_view tag{"TestTag"};

  WithdrawIdView withdrawIdView{"WithdrawId"};

  MonetaryAmount fee{"0.02", cur};
};

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossible) {
  MonetaryAmount grossAmount{1000, cur};
  bool isPercentageWithdraw = false;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto ret =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, Duration::zero());
  EXPECT_EQ(exp, ret);
}

TEST_F(ExchangeOrchestratorWithdrawTest, WithdrawPossiblePercentage) {
  MonetaryAmount grossAmount{25, cur};
  bool isPercentageWithdraw = true;
  auto exp = createWithdrawInfo(grossAmount, isPercentageWithdraw);
  auto ret =
      exchangesOrchestrator.withdraw(grossAmount, isPercentageWithdraw, fromExchange, toExchange, Duration::zero());
  EXPECT_EQ(exp, ret);
}
}  // namespace cct
