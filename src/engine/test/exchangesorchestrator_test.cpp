#include "exchangesorchestrator.hpp"

#include <gtest/gtest.h>

#include "cct_const.hpp"
#include "coincenterinfo.hpp"
#include "cryptowatchapi.hpp"
#include "exchangeprivateapi_mock.hpp"
#include "exchangepublicapi_mock.hpp"

namespace cct {

using MarketOrderBookMap = api::ExchangePublic::MarketOrderBookMap;
using MarketSet = api::ExchangePublic::MarketSet;
using Orders = api::ExchangePrivate::Orders;
using Deposit = CurrencyExchange::Deposit;
using Withdraw = CurrencyExchange::Withdraw;
using Type = CurrencyExchange::Type;
using TradeInfo = api::TradeInfo;
using OrderInfo = api::OrderInfo;
using PlaceOrderInfo = api::PlaceOrderInfo;
using ExchangePublic = api::ExchangePublic;
using ExchangePrivate = api::ExchangePrivate;
using TradedAmountsVector = ExchangesOrchestrator::TradedAmountsVector;

class ExchangeOrchestratorTest : public ::testing::Test {
 protected:
  ExchangeOrchestratorTest()
      : loadConfiguration(kDefaultDataDir, LoadConfiguration::ExchangeConfigFileType::kTest),
        coincenterInfo(settings::RunMode::kProd, loadConfiguration),
        cryptowatchAPI(coincenterInfo, settings::RunMode::kProd, Duration::max(), true),
        fiatConverter(coincenterInfo, Duration::max()),  // max to avoid real Fiat converter queries
        exchangePublic1(kSupportedExchanges[0], fiatConverter, cryptowatchAPI, coincenterInfo),
        exchangePublic2(kSupportedExchanges[1], fiatConverter, cryptowatchAPI, coincenterInfo),
        exchangePublic3(kSupportedExchanges[2], fiatConverter, cryptowatchAPI, coincenterInfo),
        key1("test1", "testuser1", "", "", ""),
        key2("test2", "testuser2", "", "", ""),
        exchangePrivate1(exchangePublic1, coincenterInfo, key1),
        exchangePrivate2(exchangePublic2, coincenterInfo, key1),
        exchangePrivate3(exchangePublic3, coincenterInfo, key1),
        exchangePrivate4(exchangePublic3, coincenterInfo, key2),
        exchange1(coincenterInfo.exchangeInfo(exchangePublic1.name()), exchangePublic1, exchangePrivate1),
        exchange2(coincenterInfo.exchangeInfo(exchangePublic2.name()), exchangePublic2, exchangePrivate2),
        exchange3(coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate3),
        exchange4(coincenterInfo.exchangeInfo(exchangePublic3.name()), exchangePublic3, exchangePrivate4),
        exchangesOrchestrator(std::span<Exchange>(&exchange1, 4)) {}

  virtual void SetUp() {
    for (MonetaryAmount a : amounts1) {
      balancePortfolio1.add(a);
    }
    for (MonetaryAmount a : amounts2) {
      balancePortfolio2.add(a);
    }
    for (MonetaryAmount a : amounts3) {
      balancePortfolio3.add(a);
    }
    for (MonetaryAmount a : amounts4) {
      balancePortfolio4.add(a);
    }
  }

  virtual void TearDown() {}

  LoadConfiguration loadConfiguration;
  CoincenterInfo coincenterInfo;
  api::CryptowatchAPI cryptowatchAPI;
  FiatConverter fiatConverter;
  api::MockExchangePublic exchangePublic1, exchangePublic2, exchangePublic3;
  api::APIKey key1, key2;
  api::MockExchangePrivate exchangePrivate1, exchangePrivate2, exchangePrivate3, exchangePrivate4;
  Exchange exchange1, exchange2, exchange3, exchange4;
  ExchangesOrchestrator exchangesOrchestrator;

  Market m1{"ETH", "EUR"};
  Market m2{"BTC", "EUR"};
  Market m3{"XRP", "BTC"};

  VolAndPriNbDecimals volAndPriDec{2, 2};
  int depth = 10;
  int64_t nbSecondsSinceEpoch = 0;

  MonetaryAmount askPrice1{"2300.45 EUR"};
  MonetaryAmount bidPrice1{"2300.4 EUR"};
  MarketOrderBook marketOrderBook10{
      askPrice1, MonetaryAmount("1.09 ETH"), bidPrice1, MonetaryAmount("41 ETH"), volAndPriDec, depth};
  MarketOrderBook marketOrderBook11{MonetaryAmount{"2301.15EUR"},
                                    MonetaryAmount("0.4 ETH"),
                                    MonetaryAmount{"2301.05EUR"},
                                    MonetaryAmount("17 ETH"),
                                    volAndPriDec,
                                    depth - 2};

  MonetaryAmount askPrice2{"31056.67 EUR"};
  MonetaryAmount bidPrice2{"31056.66 EUR"};
  MarketOrderBook marketOrderBook20{
      askPrice2, MonetaryAmount("0.12BTC"), bidPrice2, MonetaryAmount("0.00234 BTC"), volAndPriDec, depth};
  MarketOrderBook marketOrderBook21{MonetaryAmount{"31051.02 EUR"},
                                    MonetaryAmount("0.409BTC"),
                                    MonetaryAmount{"31051.01 EUR"},
                                    MonetaryAmount("1.9087 BTC"),
                                    volAndPriDec,
                                    depth + 1};

  MonetaryAmount askPrice3{"0.37 BTC"};
  MonetaryAmount bidPrice3{"0.36 BTC"};
  MarketOrderBook marketOrderBook3{
      askPrice3, MonetaryAmount("916.4XRP"), bidPrice3, MonetaryAmount("3494XRP"), volAndPriDec, depth};

  const MonetaryAmount amounts1[4] = {MonetaryAmount("1500XRP"), MonetaryAmount("15BTC"), MonetaryAmount("1.5ETH"),
                                      MonetaryAmount("5000USDT")};
  const MonetaryAmount amounts2[4] = {MonetaryAmount("37SOL"), MonetaryAmount("1887565SHIB"), MonetaryAmount("0.5BTC"),
                                      MonetaryAmount("6750USDT")};
  const MonetaryAmount amounts3[5] = {MonetaryAmount("0.6ETH"), MonetaryAmount("1000XLM"), MonetaryAmount("0.01AVAX"),
                                      MonetaryAmount("1500EUR"), MonetaryAmount("4250USDT")};
  const MonetaryAmount amounts4[6] = {MonetaryAmount("147ADA"),     MonetaryAmount("4.76DOT"),
                                      MonetaryAmount("15004MATIC"), MonetaryAmount("155USD"),
                                      MonetaryAmount("107.5USDT"),  MonetaryAmount("1200EUR")};

  BalancePortfolio balancePortfolio1, balancePortfolio2, balancePortfolio3, balancePortfolio4;
};

TEST_F(ExchangeOrchestratorTest, TickerInformation) {
  const MarketOrderBookMap marketOrderbookMap1 = {{m1, marketOrderBook10}, {m2, marketOrderBook20}};
  EXPECT_CALL(exchangePublic1, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap1));

  const MarketOrderBookMap marketOrderbookMap2 = {{m1, marketOrderBook10}, {m3, marketOrderBook3}};
  EXPECT_CALL(exchangePublic2, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap2));

  const string kTestedExchanges12[] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[1])};

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
  const string kTestedExchanges123[] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[1]),
                                        string(kSupportedExchanges[2])};

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
  const string kTestedExchanges2[] = {string(kSupportedExchanges[1])};
  EXPECT_EQ(exchangesOrchestrator.getMarketOrderBooks(testedMarket, kTestedExchanges2, equiCurrencyCode, optDepth),
            marketOrderBookConversionRates);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencyUniqueExchange) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));

  const PrivateExchangeName privateExchangeNames[1] = {PrivateExchangeName(exchange1.name(), exchange1.keyName())};
  BalancePerExchange ret{{&exchange1, balancePortfolio1}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, BalanceNoEquivalentCurrencySeveralExchanges) {
  CurrencyCode equiCurrency;

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(equiCurrency)).WillOnce(testing::Return(balancePortfolio3));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange4.name(), exchange4.keyName())};
  BalancePerExchange ret{
      {&exchange1, balancePortfolio1}, {&exchange3, balancePortfolio2}, {&exchange4, balancePortfolio3}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoUniqueExchanges) {
  CurrencyCode depositCurrency{"ETH"};

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange2.name(), exchange2.keyName())};

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

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange4.name(), exchange4.keyName())};

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

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange4.name(), exchange4.keyName())};

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

  OpenedOrdersPerExchange ret{{&exchange2, orders2}, {&exchange3, orders3}, {&exchange4, orders4}};
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

  const string kTestedExchanges13[] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[2])};

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
class ExchangeOrchestratorSimpleTradeTest : public ExchangeOrchestratorTest {
 protected:
  ExchangeOrchestratorSimpleTradeTest()
      : tradeOptions(priceOptions, TradeTimeoutAction::kCancel, TradeMode::kReal, Duration::max(), Duration::zero(),
                     TradeTypePolicy::kDefault) {
    resetMarkets();
  }

  enum class TradableMarkets : int8_t { kExpectNoCall, kExpectCall, kNoExpectation };
  enum class OrderBook : int8_t { kExpectNoCall, kExpectCall, kExpect2Calls, kNoExpectation };
  enum class AllOrderBooks : int8_t { kExpectNoCall, kExpectCall, kNoExpectation };

  TradedAmounts expectTrade(int exchangePrivateNum, MonetaryAmount from, CurrencyCode toCurrency, TradeSide side,
                            TradableMarkets tradableMarketsCall, OrderBook orderBookCall,
                            AllOrderBooks allOrderBooksCall, bool makeMarketAvailable) {
    Market m(from.currencyCode(), toCurrency);
    if (side == TradeSide::kBuy) {
      m = m.reverse();
    }

    // Choose price of 1 such that we do not need to make a division if it's a buy.
    MonetaryAmount vol(from, m.base());
    MonetaryAmount pri(1, m.quote());

    VolAndPriNbDecimals volAndPriDec{2, 2};

    MonetaryAmount maxVol(std::numeric_limits<MonetaryAmount::AmountType>::max(), m.base(), volAndPriDec.volNbDecimals);

    MonetaryAmount tradedTo(from.toNeutral(), toCurrency);

    int depth = MarketOrderBook::kDefaultDepth;
    MonetaryAmount deltaPri(1, pri.currencyCode(), volAndPriDec.priNbDecimals);
    MonetaryAmount askPrice = side == TradeSide::kBuy ? pri : pri + deltaPri;
    MonetaryAmount bidPrice = side == TradeSide::kSell ? pri : pri - deltaPri;
    MarketOrderBook marketOrderbook{askPrice, maxVol, bidPrice, maxVol, volAndPriDec, depth};

    TradedAmounts tradedAmounts(from, tradedTo);
    OrderId orderId("OrderId # 0");
    OrderInfo orderInfo(tradedAmounts, true);
    PlaceOrderInfo placeOrderInfo(orderInfo, orderId);

    if (makeMarketAvailable) {
      markets.insert(m);
      marketOrderBookMap.insert_or_assign(m, marketOrderbook);
    }

    // EXPECT_CALL does not allow references. Or I did not found the way to make it work...
    switch (exchangePrivateNum) {
      case 1:
        if (tradableMarketsCall == TradableMarkets::kExpectCall) {
          EXPECT_CALL(exchangePublic1, queryTradableMarkets()).WillOnce(testing::Return(markets));
        } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {
          EXPECT_CALL(exchangePublic1, queryTradableMarkets()).Times(0);
        }

        switch (orderBookCall) {
          case OrderBook::kExpect2Calls:
            EXPECT_CALL(exchangePublic1, queryOrderBook(m, depth))
                .Times(2)
                .WillRepeatedly(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectCall:
            EXPECT_CALL(exchangePublic1, queryOrderBook(m, depth)).WillOnce(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectNoCall:
            EXPECT_CALL(exchangePublic1, queryOrderBook(m, depth)).Times(0);
            break;
          case OrderBook::kNoExpectation:
            break;
        }

        if (allOrderBooksCall == AllOrderBooks::kExpectCall) {
          EXPECT_CALL(exchangePublic1, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));
        } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {
          EXPECT_CALL(exchangePublic1, queryAllApproximatedOrderBooks(1)).Times(0);
        }

        EXPECT_CALL(exchangePrivate1, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
        if (makeMarketAvailable && from.isStrictlyPositive()) {
          EXPECT_CALL(exchangePrivate1, placeOrder(from, vol, pri, testing::_))
              .WillOnce(testing::Return(placeOrderInfo));
        } else {
          EXPECT_CALL(exchangePrivate1, placeOrder(from, vol, pri, testing::_)).Times(0);
        }
        break;
      case 2:
        if (tradableMarketsCall == TradableMarkets::kExpectCall) {
          EXPECT_CALL(exchangePublic2, queryTradableMarkets()).WillOnce(testing::Return(markets));
        } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {
          EXPECT_CALL(exchangePublic2, queryTradableMarkets()).Times(0);
        }

        switch (orderBookCall) {
          case OrderBook::kExpect2Calls:
            EXPECT_CALL(exchangePublic2, queryOrderBook(m, depth))
                .Times(2)
                .WillRepeatedly(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectCall:
            EXPECT_CALL(exchangePublic2, queryOrderBook(m, depth)).WillOnce(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectNoCall:
            EXPECT_CALL(exchangePublic2, queryOrderBook(m, depth)).Times(0);
            break;
          case OrderBook::kNoExpectation:
            break;
        }

        if (allOrderBooksCall == AllOrderBooks::kExpectCall) {
          EXPECT_CALL(exchangePublic2, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));
        } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {
          EXPECT_CALL(exchangePublic2, queryAllApproximatedOrderBooks(1)).Times(0);
        }

        EXPECT_CALL(exchangePrivate2, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
        if (makeMarketAvailable && from.isStrictlyPositive()) {
          EXPECT_CALL(exchangePrivate2, placeOrder(from, vol, pri, testing::_))
              .WillOnce(testing::Return(placeOrderInfo));
        } else {
          EXPECT_CALL(exchangePrivate2, placeOrder(from, vol, pri, testing::_)).Times(0);
        }
        break;
      case 3:
        if (tradableMarketsCall == TradableMarkets::kExpectCall) {
          EXPECT_CALL(exchangePublic3, queryTradableMarkets()).WillOnce(testing::Return(markets));
        } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {
          EXPECT_CALL(exchangePublic3, queryTradableMarkets()).Times(0);
        }

        switch (orderBookCall) {
          case OrderBook::kExpect2Calls:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth))
                .Times(2)
                .WillRepeatedly(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectCall:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth)).WillOnce(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectNoCall:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth)).Times(0);
            break;
          case OrderBook::kNoExpectation:
            break;
        }

        if (allOrderBooksCall == AllOrderBooks::kExpectCall) {
          EXPECT_CALL(exchangePublic3, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));
        } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {
          EXPECT_CALL(exchangePublic3, queryAllApproximatedOrderBooks(1)).Times(0);
        }

        EXPECT_CALL(exchangePrivate3, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
        if (makeMarketAvailable && from.isStrictlyPositive()) {
          EXPECT_CALL(exchangePrivate3, placeOrder(from, vol, pri, testing::_))
              .WillOnce(testing::Return(placeOrderInfo));
        } else {
          EXPECT_CALL(exchangePrivate3, placeOrder(from, vol, pri, testing::_)).Times(0);
        }
        break;
      case 4:
        if (tradableMarketsCall == TradableMarkets::kExpectCall) {
          EXPECT_CALL(exchangePublic3, queryTradableMarkets()).WillOnce(testing::Return(markets));
        } else if (tradableMarketsCall == TradableMarkets::kExpectNoCall) {
          EXPECT_CALL(exchangePublic3, queryTradableMarkets()).Times(0);
        }

        switch (orderBookCall) {
          case OrderBook::kExpect2Calls:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth))
                .Times(2)
                .WillRepeatedly(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectCall:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth)).WillOnce(testing::Return(marketOrderbook));
            break;
          case OrderBook::kExpectNoCall:
            EXPECT_CALL(exchangePublic3, queryOrderBook(m, depth)).Times(0);
            break;
          case OrderBook::kNoExpectation:
            break;
        }

        if (allOrderBooksCall == AllOrderBooks::kExpectCall) {
          EXPECT_CALL(exchangePublic3, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderBookMap));
        } else if (allOrderBooksCall == AllOrderBooks::kExpectNoCall) {
          EXPECT_CALL(exchangePublic3, queryAllApproximatedOrderBooks(1)).Times(0);
        }

        EXPECT_CALL(exchangePrivate4, isSimulatedOrderSupported()).WillRepeatedly(testing::Return(false));
        if (makeMarketAvailable && from.isStrictlyPositive()) {
          EXPECT_CALL(exchangePrivate4, placeOrder(from, vol, pri, testing::_))
              .WillOnce(testing::Return(placeOrderInfo));
        } else {
          EXPECT_CALL(exchangePrivate4, placeOrder(from, vol, pri, testing::_)).Times(0);
        }
        break;
    }

    return tradedAmounts;
  }

  void resetMarkets() {
    marketOrderBookMap = {};
    markets = {Market("AAA", "BBB"), Market("CCC", "BBB"), Market("XXX", "ZZZ")};
  }

  PriceOptions priceOptions{PriceStrategy::kTaker};
  TradeOptions tradeOptions;
  bool isPercentageTrade = false;
  api::ExchangePublic::MarketOrderBookMap marketOrderBookMap;
  MarketSet markets;
};

TEST_F(ExchangeOrchestratorSimpleTradeTest, SingleExchangeBuy) {
  MonetaryAmount from(100, "EUR");
  CurrencyCode toCurrency("XRP");
  TradeSide side = TradeSide::kBuy;
  TradedAmounts tradedAmounts = expectTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                            OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, NoAvailableAmountToSell) {
  MonetaryAmount from(10, "SOL");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));

  MonetaryAmount zero(0, from.currencyCode());
  expectTrade(2, zero, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, false);
  expectTrade(1, zero, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, true);

  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            TradedAmounts(from.currencyCode(), toCurrency));
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoAccountsSameExchangeSell) {
  MonetaryAmount from(2, "ETH");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange4.name(), exchange4.keyName())};

  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  MonetaryAmount ratio3("0.75");
  MonetaryAmount ratio4 = MonetaryAmount(1) - ratio3;
  TradedAmounts tradedAmounts3 = expectTrade(3, from * ratio3, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectTrade(4, from * ratio4, toCurrency, side, TradableMarkets::kNoExpectation,
                                             OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts = tradedAmounts3 + tradedAmounts4;
  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, ThreeExchangesBuy) {
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

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts2 = expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  TradedAmounts tradedAmounts = tradedAmounts1 + tradedAmounts2 + tradedAmounts3;
  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, PrivateExchangeNames{}, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, ThreeExchangesBuyNotEnoughAmount) {
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
  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectNoCall, AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts2 = expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation,
                                             OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  TradedAmounts tradedAmounts = tradedAmounts1 + tradedAmounts2 + tradedAmounts3 + tradedAmounts4;
  EXPECT_EQ(exchangesOrchestrator.trade(from, isPercentageTrade, toCurrency, PrivateExchangeNames{}, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, SingleExchangeBuyAll) {
  CurrencyCode fromCurrency("EUR");
  CurrencyCode toCurrency("XRP");
  TradeSide side = TradeSide::kBuy;

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName())};

  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  TradedAmounts tradedAmounts =
      expectTrade(3, MonetaryAmount(1500, fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                  OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_EQ(exchangesOrchestrator.tradeAll(fromCurrency, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoExchangesSellAll) {
  CurrencyCode fromCurrency("ETH");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange3.name(), exchange3.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  TradedAmounts tradedAmounts1 =
      expectTrade(1, balancePortfolio1.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                  OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 =
      expectTrade(3, balancePortfolio3.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                  OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  TradedAmounts tradedAmounts = tradedAmounts1 + tradedAmounts3;
  EXPECT_EQ(exchangesOrchestrator.tradeAll(fromCurrency, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, AllExchangesBuyAllOneMarketUnavailable) {
  CurrencyCode fromCurrency("USDT");
  CurrencyCode toCurrency("DOT");
  TradeSide side = TradeSide::kBuy;

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange4.name(), exchange4.keyName())};

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  expectTrade(1, MonetaryAmount(0, fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
              OrderBook::kExpectNoCall, AllOrderBooks::kExpectNoCall, false);

  TradedAmounts tradedAmounts2 =
      expectTrade(2, balancePortfolio2.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                  OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 =
      expectTrade(3, balancePortfolio3.get(fromCurrency), toCurrency, side, TradableMarkets::kExpectCall,
                  OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 =
      expectTrade(4, balancePortfolio4.get(fromCurrency), toCurrency, side, TradableMarkets::kNoExpectation,
                  OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  TradedAmounts tradedAmounts = tradedAmounts2 + tradedAmounts3 + tradedAmounts4;
  EXPECT_EQ(exchangesOrchestrator.tradeAll(fromCurrency, toCurrency, privateExchangeNames, tradeOptions),
            tradedAmounts);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, SingleExchangeSmartBuy) {
  // Fee is automatically applied on buy
  MonetaryAmount endAmount = MonetaryAmount(1000, "XRP") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from = MonetaryAmount(1000, "USDT");

  TradedAmounts tradedAmounts = expectTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                            OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsVector ret{tradedAmounts};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), ret);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoExchangesSmartBuy) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from31 = MonetaryAmount(4250, "USDT");
  MonetaryAmount from32 = MonetaryAmount(750, "EUR");

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts31 = expectTrade(3, from31, toCurrency, side, TradableMarkets::kNoExpectation,
                                              OrderBook::kExpectCall, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts32 = expectTrade(3, from32, toCurrency, side, TradableMarkets::kExpectCall,
                                              OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsVector ret{tradedAmounts1, tradedAmounts31, tradedAmounts32};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), ret);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoExchangesSmartBuyNoMarketOnOneExchange) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(0, "USDT");
  MonetaryAmount from3 = MonetaryAmount(4250, "USDT");

  expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts3 = expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsVector ret{tradedAmounts3};
  EXPECT_EQ(exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions), ret);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, ThreeExchangesSmartBuy) {
  MonetaryAmount endAmount = MonetaryAmount(10000, "XLM") * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  CurrencyCode toCurrency = endAmount.currencyCode();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from2 = MonetaryAmount(0, "USDT");
  MonetaryAmount from41 = MonetaryAmount(0, "USDT");
  MonetaryAmount from42 = MonetaryAmount(1200, "EUR");

  expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, false);

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  resetMarkets();

  expectTrade(4, from41, toCurrency, side, TradableMarkets::kNoExpectation, OrderBook::kExpectNoCall,
              AllOrderBooks::kNoExpectation, false);

  TradedAmounts tradedAmounts4 = expectTrade(4, from42, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange4.name(), exchange4.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsVector ret{tradedAmounts1, tradedAmounts4};
  EXPECT_TRUE(
      std::ranges::is_permutation(ret, exchangesOrchestrator.smartBuy(endAmount, privateExchangeNames, tradeOptions)));
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, SmartBuyAllExchanges) {
  CurrencyCode toCurrency("XLM");
  MonetaryAmount endAmount = MonetaryAmount(18800, toCurrency) * exchangePublic1.exchangeInfo().getTakerFeeRatio();
  TradeSide side = TradeSide::kBuy;

  MonetaryAmount from1 = MonetaryAmount(5000, "USDT");
  MonetaryAmount from2 = MonetaryAmount(6750, "USDT");
  MonetaryAmount from31 = MonetaryAmount(1500, "EUR");
  MonetaryAmount from32 = MonetaryAmount(4250, "USDT");
  MonetaryAmount from41 = MonetaryAmount(100, "USDT");
  MonetaryAmount from42 = MonetaryAmount(1200, "EUR");

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts2 = expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts31 = expectTrade(3, from31, toCurrency, side, TradableMarkets::kExpectCall,
                                              OrderBook::kExpect2Calls, AllOrderBooks::kExpectCall, true);
  TradedAmounts tradedAmounts32 = expectTrade(3, from32, toCurrency, side, TradableMarkets::kNoExpectation,
                                              OrderBook::kExpect2Calls, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts41 = expectTrade(4, from41, toCurrency, side, TradableMarkets::kNoExpectation,
                                              OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);
  TradedAmounts tradedAmounts42 = expectTrade(4, from42, toCurrency, side, TradableMarkets::kNoExpectation,
                                              OrderBook::kNoExpectation, AllOrderBooks::kNoExpectation, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  TradedAmountsVector ret{tradedAmounts1,  tradedAmounts2,  tradedAmounts31,
                          tradedAmounts32, tradedAmounts41, tradedAmounts42};
  EXPECT_TRUE(std::ranges::is_permutation(
      ret, exchangesOrchestrator.smartBuy(endAmount, PrivateExchangeNames{}, tradeOptions)));
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, SingleExchangeSmartSell) {
  MonetaryAmount startAmount = MonetaryAmount(2, "ETH");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from = MonetaryAmount("1.5ETH");

  TradedAmounts tradedAmounts = expectTrade(1, from, toCurrency, side, TradableMarkets::kExpectCall,
                                            OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName())};

  TradedAmountsVector ret{tradedAmounts};
  EXPECT_EQ(exchangesOrchestrator.smartSell(startAmount, privateExchangeNames, tradeOptions), ret);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoExchangesSmartSell) {
  MonetaryAmount startAmount = MonetaryAmount(16, "BTC");
  CurrencyCode fromCurrency = startAmount.currencyCode();
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(15, fromCurrency);
  MonetaryAmount from2 = MonetaryAmount("0.5", fromCurrency);

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts2 = expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange2.name(), exchange2.keyName())};

  TradedAmountsVector ret{tradedAmounts1, tradedAmounts2};
  EXPECT_TRUE(std::ranges::is_permutation(
      ret, exchangesOrchestrator.smartSell(startAmount, privateExchangeNames, tradeOptions)));
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, TwoExchangesSmartSellNoMarketOnOneExchange) {
  MonetaryAmount startAmount = MonetaryAmount(10000, "SHIB");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from2 = startAmount;
  MonetaryAmount from3 = MonetaryAmount(0, startAmount.currencyCode());

  expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, false);
  TradedAmounts tradedAmounts2 = expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                      PrivateExchangeName(exchange3.name(), exchange3.keyName())};

  TradedAmountsVector ret{tradedAmounts2};
  EXPECT_EQ(exchangesOrchestrator.smartSell(startAmount, privateExchangeNames, tradeOptions), ret);
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, ThreeExchangesSmartSellFromAnotherPreferredCurrency) {
  MonetaryAmount startAmount = MonetaryAmount(2000, "EUR");
  CurrencyCode toCurrency("USDT");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from3 = MonetaryAmount(1500, startAmount.currencyCode());
  MonetaryAmount from4 = MonetaryAmount(500, startAmount.currencyCode());

  expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts3 = expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpect2Calls, AllOrderBooks::kExpectNoCall, true);
  TradedAmounts tradedAmounts4 = expectTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation,
                                             OrderBook::kNoExpectation, AllOrderBooks::kExpectNoCall, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  const PrivateExchangeName privateExchangeNames[] = {PrivateExchangeName(exchange4.name(), exchange4.keyName()),
                                                      PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                      PrivateExchangeName(exchange3.name(), exchange3.keyName())};

  TradedAmountsVector ret{tradedAmounts3, tradedAmounts4};
  EXPECT_TRUE(std::ranges::is_permutation(
      ret, exchangesOrchestrator.smartSell(startAmount, privateExchangeNames, tradeOptions)));
}

TEST_F(ExchangeOrchestratorSimpleTradeTest, SmartSellAllExchanges) {
  MonetaryAmount startAmount = MonetaryAmount(1, "ETH");
  CurrencyCode toCurrency("EUR");
  TradeSide side = TradeSide::kSell;

  MonetaryAmount from1 = MonetaryAmount(1, startAmount.currencyCode());
  MonetaryAmount from2 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from3 = MonetaryAmount(0, startAmount.currencyCode());
  MonetaryAmount from4 = MonetaryAmount(0, startAmount.currencyCode());

  TradedAmounts tradedAmounts1 = expectTrade(1, from1, toCurrency, side, TradableMarkets::kExpectCall,
                                             OrderBook::kExpectCall, AllOrderBooks::kExpectNoCall, true);
  expectTrade(2, from2, toCurrency, side, TradableMarkets::kExpectNoCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, true);
  expectTrade(3, from3, toCurrency, side, TradableMarkets::kExpectCall, OrderBook::kExpectNoCall,
              AllOrderBooks::kExpectNoCall, true);
  expectTrade(4, from4, toCurrency, side, TradableMarkets::kNoExpectation, OrderBook::kNoExpectation,
              AllOrderBooks::kNoExpectation, true);

  EXPECT_CALL(exchangePrivate1, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio1));
  EXPECT_CALL(exchangePrivate2, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio2));
  EXPECT_CALL(exchangePrivate3, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio3));
  EXPECT_CALL(exchangePrivate4, queryAccountBalance(testing::_)).WillOnce(testing::Return(balancePortfolio4));

  TradedAmountsVector ret{tradedAmounts1};
  EXPECT_EQ(ret, exchangesOrchestrator.smartSell(startAmount, PrivateExchangeNames{}, tradeOptions));
}

TEST_F(ExchangeOrchestratorTest, WithdrawSameAccountImpossible) {
  MonetaryAmount grossAmount{1000, "XRP"};
  PrivateExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  PrivateExchangeName toExchange = fromExchange;
  EXPECT_THROW(exchangesOrchestrator.withdraw(grossAmount, fromExchange, toExchange), exception);
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleFrom) {
  MonetaryAmount grossAmount{1000, "XRP"};
  PrivateExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  PrivateExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, fromExchange, toExchange).hasBeenInitiated());
}

TEST_F(ExchangeOrchestratorTest, WithdrawImpossibleTo) {
  MonetaryAmount grossAmount{1000, "XRP"};
  PrivateExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  PrivateExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  EXPECT_FALSE(exchangesOrchestrator.withdraw(grossAmount, fromExchange, toExchange).hasBeenInitiated());
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

TEST_F(ExchangeOrchestratorTest, WithdrawPossible) {
  MonetaryAmount grossAmount{1000, "XRP"};
  CurrencyCode cur = grossAmount.currencyCode();
  PrivateExchangeName fromExchange(exchange1.name(), exchange1.keyName());
  PrivateExchangeName toExchange(exchange2.name(), exchange2.keyName());

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kUnavailable, Withdraw::kAvailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));
  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(grossAmount.currencyCode(), Deposit::kAvailable, Withdraw::kUnavailable, Type::kCrypto),
      CurrencyExchange("SHIB", Deposit::kAvailable, Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  std::string_view address = "TestAddress";
  std::string_view tag = "TestTag";
  Wallet receivingWallet(toExchange, cur, address, tag, WalletCheck());
  EXPECT_CALL(exchangePrivate2, queryDepositWallet(cur)).WillOnce(testing::Return(receivingWallet));

  WithdrawIdView withdrawIdView = "WithdrawId";
  api::InitiatedWithdrawInfo initiatedWithdrawInfo(receivingWallet, withdrawIdView, grossAmount);
  EXPECT_CALL(exchangePrivate1, launchWithdraw(grossAmount, std::move(receivingWallet)))
      .WillOnce(testing::Return(initiatedWithdrawInfo));

  MonetaryAmount fee("0.02 XRP");
  MonetaryAmount netEmittedAmount = grossAmount - fee;

  api::SentWithdrawInfo unsentWithdrawInfo(netEmittedAmount, false);
  api::SentWithdrawInfo sentWithdrawInfo(netEmittedAmount, true);
  EXPECT_CALL(exchangePrivate1, isWithdrawSuccessfullySent(initiatedWithdrawInfo))
      .WillOnce(testing::Return(sentWithdrawInfo));

  EXPECT_CALL(exchangePrivate2, isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo))
      .WillOnce(testing::Return(true));

  WithdrawInfo withdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
  EXPECT_EQ(exchangesOrchestrator.withdraw(grossAmount, fromExchange, toExchange, Duration::zero()), withdrawInfo);
}
}  // namespace cct