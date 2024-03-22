#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <optional>
#include <span>

#include "cct_const.hpp"
#include "currencycode.hpp"
#include "currencyexchange.hpp"
#include "currencyexchangeflatset.hpp"
#include "exchange-names.hpp"
#include "exchange.hpp"
#include "exchangedata_test.hpp"
#include "exchangename.hpp"
#include "exchangepublicapitypes.hpp"
#include "exchangeretriever.hpp"
#include "exchangesorchestrator.hpp"
#include "market.hpp"
#include "queryresulttypes.hpp"

namespace cct {

using Type = CurrencyExchange::Type;
using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

class ExchangeOrchestratorTest : public ExchangesBaseTest {
 protected:
  ExchangesOrchestrator exchangesOrchestrator{RequestsConfig{}, std::span<Exchange>(&this->exchange1, 8)};
};

TEST_F(ExchangeOrchestratorTest, HealthCheck) {
  EXPECT_CALL(exchangePublic1, healthCheck()).WillOnce(testing::Return(true));
  EXPECT_CALL(exchangePublic2, healthCheck()).WillOnce(testing::Return(false));

  const ExchangeName kTestedExchanges12[] = {ExchangeName(kSupportedExchanges[0]),
                                             ExchangeName(kSupportedExchanges[1])};

  ExchangeHealthCheckStatus expectedHealthCheck = {{&exchange1, true}, {&exchange2, false}};
  EXPECT_EQ(exchangesOrchestrator.healthCheck(kTestedExchanges12), expectedHealthCheck);
}

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

TEST_F(ExchangeOrchestratorTest, GetMarketsPerExchangeNoCurrency) {
  CurrencyCode cur1{};
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

  MarketsPerExchange ret{{&exchange1, MarketSet{m1, m2, m4, m6}},
                         {&exchange2, MarketSet{m1, m2, m3, m4, m5, m6}},
                         {&exchange3, MarketSet{m1, m2, m6}}};
  EXPECT_EQ(exchangesOrchestrator.getMarketsPerExchange(cur1, cur2, exchangeNameSpan), ret);
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
      CurrencyExchangeVector{CurrencyExchange("XRP", CurrencyExchange::Deposit::kUnavailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto),
                             CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable,
                                              CurrencyExchange::Withdraw::kAvailable, Type::kCrypto)}};
  EXPECT_CALL(ExchangePrivate(exchange1), queryTradableCurrencies())
      .Times(2)
      .WillRepeatedly(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
                       Type::kCrypto),
      CurrencyExchange("SOL", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       Type::kCrypto),
      CurrencyExchange("XRP", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kUnavailable,
                       Type::kCrypto),
      CurrencyExchange("EUR", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       Type::kFiat),
  }};
  EXPECT_CALL(ExchangePrivate(exchange3), queryTradableCurrencies())
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
}  // namespace cct
