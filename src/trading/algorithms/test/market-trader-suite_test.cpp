#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <string_view>

#include "exchange-config.hpp"
#include "market-order-book-vector.hpp"
#include "market-trader-engine.hpp"
#include "market-trader-factory.hpp"
#include "mean-reversion-trader.hpp"
#include "rsi-trader.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "public-trade-vector.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"
#include "volumeandpricenbdecimals.hpp"

namespace cct {

namespace {
// Builds a simple two-sided order book centered on 'mid' EUR with deep liquidity on each side,
// so a small taker/nibble order fully matches.
MarketOrderBook MakeOrderBook(TimePoint ts, double mid) {
  const MonetaryAmount askPrice(mid + 0.5, "EUR");
  const MonetaryAmount bidPrice(mid - 0.5, "EUR");
  const MonetaryAmount volume(1000, "SOL");
  return MarketOrderBook(ts, askPrice, volume, bidPrice, volume, VolAndPriNbDecimals{2, 2});
}
}  // namespace

class MarketTraderSuiteTest : public ::testing::Test {
 protected:
  schema::ExchangeConfig exchangeConfig;  // default config => zero trade fees
  Market market{"SOL", "EUR"};
  MarketTraderFactory factory;
  TimePoint t0{Clock::now()};
};

// Regression test: a taker BUY used to add a quote amount to the base balance, throwing
// "Addition is only possible on amounts with same currency". buy-and-hold issues a taker buy.
TEST_F(MarketTraderSuiteTest, TakerBuyDoesNotMixCurrenciesAndAcquiresBase) {
  MarketTraderEngine engine(exchangeConfig, market, MonetaryAmount("0SOL"), MonetaryAmount("1000EUR"));
  engine.registerMarketTrader(factory.construct("buy-and-hold", engine.marketTraderEngineState()));

  MarketOrderBookVector orderBooks;
  orderBooks.push_back(MakeOrderBook(t0, 100.0));
  orderBooks.push_back(MakeOrderBook(t0 + std::chrono::seconds(2), 101.0));

  ASSERT_NO_THROW(engine.tradeRange(std::move(orderBooks), PublicTradeVector{}));

  const auto& state = engine.marketTraderEngineState();
  EXPECT_GT(state.availableBaseAmount(), MonetaryAmount("0SOL"));
  EXPECT_LT(state.availableQuoteAmount(), MonetaryAmount("1000EUR"));
  EXPECT_NO_THROW((void)engine.finalizeAndComputeResult());
}

// Grid buys a slice when the price drops one step and sells a slice when it rises one step.
TEST_F(MarketTraderSuiteTest, GridBuysOnDipSellsOnRise) {
  MarketTraderEngine engine(exchangeConfig, market, MonetaryAmount("10SOL"), MonetaryAmount("1000EUR"));
  engine.registerMarketTrader(factory.construct("grid", engine.marketTraderEngineState()));

  MarketOrderBookVector orderBooks;
  orderBooks.push_back(MakeOrderBook(t0, 100.0));                              // sets anchor at 100
  orderBooks.push_back(MakeOrderBook(t0 + std::chrono::seconds(2), 98.0));    // -2% => buy
  orderBooks.push_back(MakeOrderBook(t0 + std::chrono::seconds(4), 100.0));   // +~2% => sell

  engine.tradeRange(std::move(orderBooks), PublicTradeVector{});

  const auto closedOrders = engine.marketTraderEngineState().closedOrders();
  bool hasBuy = false;
  bool hasSell = false;
  for (const auto& order : closedOrders) {
    hasBuy |= order.side() == TradeSide::buy;
    hasSell |= order.side() == TradeSide::sell;
  }
  EXPECT_TRUE(hasBuy);
  EXPECT_TRUE(hasSell);
}

// Mean-reversion buys when the price is far below its recent moving average (strongly negative z-score).
TEST_F(MarketTraderSuiteTest, MeanReversionBuysOnStrongDip) {
  MarketTraderEngine engine(exchangeConfig, market, MonetaryAmount("0SOL"), MonetaryAmount("1000EUR"));
  // Use a short lookback so the 40-minute synthetic series below covers a full window.
  MeanReversionMarketTrader::Parameters params;
  params.lookback = std::chrono::minutes(30);
  params.entryZ = 1.5;
  engine.registerMarketTrader(
      std::make_unique<MeanReversionMarketTrader>("mean-reversion", engine.marketTraderEngineState(), params));

  MarketOrderBookVector orderBooks;
  // 20 stable points spread over 40 minutes (covers the 30-minute lookback), then a sharp drop.
  for (int i = 0; i < 20; ++i) {
    orderBooks.push_back(MakeOrderBook(t0 + std::chrono::minutes(2 * i), 100.0));
  }
  orderBooks.push_back(MakeOrderBook(t0 + std::chrono::minutes(40), 80.0));  // big dip => z << -1.5 => buy

  engine.tradeRange(std::move(orderBooks), PublicTradeVector{});

  EXPECT_GT(engine.marketTraderEngineState().availableBaseAmount(), MonetaryAmount("0SOL"));
}

// RSI buys when the market is strongly oversold (a sustained decline drives RSI toward 0).
TEST_F(MarketTraderSuiteTest, RsiBuysWhenOversold) {
  MarketTraderEngine engine(exchangeConfig, market, MonetaryAmount("0SOL"), MonetaryAmount("1000EUR"));
  RsiMarketTrader::Parameters params;
  params.rsiPeriods = 14;
  params.samplingPeriod = std::chrono::minutes(1);
  params.decisionInterval = std::chrono::minutes(1);
  params.priceStrategy = PriceStrategy::nibble;  // ensure an immediate fill for the assertion
  engine.registerMarketTrader(
      std::make_unique<RsiMarketTrader>("rsi", engine.marketTraderEngineState(), params));

  MarketOrderBookVector orderBooks;
  // 30 steadily declining points, one per minute -> only losses -> RSI ~ 0 -> oversold -> buy.
  for (int i = 0; i < 30; ++i) {
    orderBooks.push_back(MakeOrderBook(t0 + std::chrono::minutes(i), 130.0 - i));
  }

  engine.tradeRange(std::move(orderBooks), PublicTradeVector{});

  EXPECT_GT(engine.marketTraderEngineState().availableBaseAmount(), MonetaryAmount("0SOL"));
}

}  // namespace cct
