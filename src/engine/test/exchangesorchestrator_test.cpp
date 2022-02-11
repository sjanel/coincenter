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

class ExchangeOrchestratorTest : public ::testing::Test {
 protected:
  static_assert(kNbSupportedExchanges >= 3);

  const string kTestedExchanges2[1] = {string(kSupportedExchanges[1])};
  const string kTestedExchanges12[2] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[1])};
  const string kTestedExchanges13[2] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[2])};
  const string kTestedExchanges123[3] = {string(kSupportedExchanges[0]), string(kSupportedExchanges[1]),
                                         string(kSupportedExchanges[2])};

  ExchangeOrchestratorTest()
      : cryptowatchAPI(coincenterInfo, settings::RunMode::kProd, Duration::max(), true),
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
  }

  virtual void TearDown() {}

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

  const MonetaryAmount amounts1[3] = {MonetaryAmount("1500XRP"), MonetaryAmount("15BTC"), MonetaryAmount("0.45 ETH")};
  const MonetaryAmount amounts2[3] = {MonetaryAmount("37SOL"), MonetaryAmount("1887565SHIB"), MonetaryAmount("0.5BTC")};
  const MonetaryAmount amounts3[4] = {MonetaryAmount("1.55ETH"), MonetaryAmount("1000XLM"), MonetaryAmount("0.01AVAX"),
                                      MonetaryAmount("1500EUR")};

  BalancePortfolio balancePortfolio1, balancePortfolio2, balancePortfolio3;
};

TEST_F(ExchangeOrchestratorTest, TickerInformation) {
  const MarketOrderBookMap marketOrderbookMap1 = {{m1, marketOrderBook10}, {m2, marketOrderBook20}};
  EXPECT_CALL(exchangePublic1, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap1));

  const MarketOrderBookMap marketOrderbookMap2 = {{m1, marketOrderBook10}, {m3, marketOrderBook3}};
  EXPECT_CALL(exchangePublic2, queryAllApproximatedOrderBooks(1)).WillOnce(testing::Return(marketOrderbookMap2));

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

  const PrivateExchangeName privateExchangeNames[3] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                       PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                       PrivateExchangeName(exchange4.name(), exchange4.keyName())};
  BalancePerExchange ret{
      {&exchange1, balancePortfolio1}, {&exchange3, balancePortfolio2}, {&exchange4, balancePortfolio3}};
  EXPECT_EQ(exchangesOrchestrator.getBalance(privateExchangeNames, equiCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoUniqueExchanges) {
  CurrencyCode depositCurrency{"ETH"};

  const PrivateExchangeName privateExchangeNames[1] = {PrivateExchangeName(exchange2.name(), exchange2.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto),
      CurrencyExchange("XRP", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  Wallet wallet2{privateExchangeNames[0], depositCurrency, "address1", "", WalletCheck()};
  EXPECT_CALL(exchangePrivate2, queryDepositWallet(depositCurrency)).WillOnce(testing::Return(wallet2));

  WalletPerExchange ret{{&exchange2, wallet2}};
  EXPECT_EQ(exchangesOrchestrator.getDepositInfo(privateExchangeNames, depositCurrency), ret);
}

TEST_F(ExchangeOrchestratorTest, DepositInfoSeveralExchangesWithUnavailableDeposits) {
  CurrencyCode depositCurrency{"XRP"};

  const PrivateExchangeName privateExchangeNames[4] = {PrivateExchangeName(exchange1.name(), exchange1.keyName()),
                                                       PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                       PrivateExchangeName(exchange3.name(), exchange3.keyName()),
                                                       PrivateExchangeName(exchange4.name(), exchange4.keyName())};

  CurrencyExchangeFlatSet tradableCurrencies1{CurrencyExchangeVector{
      CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto),
      CurrencyExchange("SHIB", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate1, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies1));

  CurrencyExchangeFlatSet tradableCurrencies2{CurrencyExchangeVector{
      CurrencyExchange("XLM", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto)}};
  EXPECT_CALL(exchangePrivate2, queryTradableCurrencies()).WillOnce(testing::Return(tradableCurrencies2));

  CurrencyExchangeFlatSet tradableCurrencies3{CurrencyExchangeVector{
      CurrencyExchange("BTC", CurrencyExchange::Deposit::kUnavailable, CurrencyExchange::Withdraw::kUnavailable,
                       CurrencyExchange::Type::kCrypto),
      CurrencyExchange("SOL", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kCrypto),
      CurrencyExchange(depositCurrency, CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kUnavailable,
                       CurrencyExchange::Type::kCrypto),
      CurrencyExchange("EUR", CurrencyExchange::Deposit::kAvailable, CurrencyExchange::Withdraw::kAvailable,
                       CurrencyExchange::Type::kFiat),
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

  const PrivateExchangeName privateExchangeNames[3] = {PrivateExchangeName(exchange2.name(), exchange2.keyName()),
                                                       PrivateExchangeName(exchange3.name(), exchange3.keyName()),
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

}  // namespace cct