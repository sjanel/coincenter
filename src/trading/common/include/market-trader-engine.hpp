#pragma once

#include <cstdint>
#include <memory>
#include <type_traits>

#include "abstract-market-trader.hpp"
#include "cct_type_traits.hpp"
#include "exchange-config.hpp"
#include "exchangeprivateapitypes.hpp"
#include "market-order-book-vector.hpp"
#include "market-trader-engine-state.hpp"
#include "market-trading-result.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "public-trade-vector.hpp"
#include "trade-range-stats.hpp"
#include "trader-command.hpp"

namespace cct {

class MarketTraderEngine {
 public:
  MarketTraderEngine(const schema::ExchangeConfig &exchangeConfig, Market market, MonetaryAmount startAmountBase,
                     MonetaryAmount startAmountQuote);

  Market market() const { return {_startAmountBase.currencyCode(), _startAmountQuote.currencyCode()}; }

  void registerMarketTrader(std::unique_ptr<AbstractMarketTrader> marketTrader);

  TradeRangeStats validateRange(MarketOrderBookVector &marketOrderBooks, PublicTradeVector &publicTrades);

  TradeRangeStats validateRange(MarketOrderBookVector &&marketOrderBooks, PublicTradeVector &&publicTrades);

  TradeRangeStats tradeRange(MarketOrderBookVector &&marketOrderBooks, PublicTradeVector &&publicTrades);

  const MarketTraderEngineState &marketTraderEngineState() const { return _marketTraderEngineState; }

  MarketTradingResult finalizeAndComputeResult();

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<MarketOrderBook> && is_trivially_relocatable_v<OpenedOrderVector> &&
                         is_trivially_relocatable_v<MarketTraderEngineState>>::type;

 private:
  void buy(const MarketOrderBook &marketOrderBook, MonetaryAmount from, PriceStrategy priceStrategy);
  void sell(const MarketOrderBook &marketOrderBook, MonetaryAmount volume, PriceStrategy priceStrategy);

  void updatePrice(const MarketOrderBook &marketOrderBook, TraderCommand traderCommand);

  void cancelCommand(int32_t orderId);

  void checkOpenedOrdersMatching(const MarketOrderBook &marketOrderBook);

  MonetaryAmount _startAmountBase;
  MonetaryAmount _startAmountQuote;
  const schema::ExchangeConfig &_exchangeConfig;
  std::unique_ptr<AbstractMarketTrader> _marketTrader;
  Market _market;
  MarketTraderEngineState _marketTraderEngineState;
  OpenedOrderVector _newlyClosedOrders;
  MarketOrderBook _lastMarketOrderBook;
};
}  // namespace cct