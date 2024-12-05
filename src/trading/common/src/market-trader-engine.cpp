#include "market-trader-engine.hpp"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <type_traits>
#include <utility>

#include "abstract-market-trader.hpp"
#include "cct_exception.hpp"
#include "cct_log.hpp"
#include "exchange-config.hpp"
#include "exchange-tradefees-config.hpp"
#include "market-data-view.hpp"
#include "market-order-book-vector.hpp"
#include "market-trading-result.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "monetaryamount.hpp"
#include "priceoptionsdef.hpp"
#include "public-trade-vector.hpp"
#include "publictrade.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "trade-range-stats.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {

MarketTraderEngine::MarketTraderEngine(const schema::ExchangeConfig &exchangeConfig, Market market,
                                       MonetaryAmount startAmountBase, MonetaryAmount startAmountQuote)
    : _startAmountBase(startAmountBase),
      _startAmountQuote(startAmountQuote),
      _exchangeConfig(exchangeConfig),
      _market(market),
      _marketTraderEngineState(startAmountBase, startAmountQuote) {
  if (market != this->market()) {
    throw exception("Inconsistent market {} and start amounts {} & {} for MarketTraderEngine", market, startAmountBase,
                    startAmountQuote);
  }
}

void MarketTraderEngine::registerMarketTrader(std::unique_ptr<AbstractMarketTrader> marketTrader) {
  if (_marketTrader) {
    throw exception("Cannot register twice a market trader to this MarketTraderEngine");
  }
  _marketTrader.swap(marketTrader);
}

namespace {

template <class VectorType>
TradeRangeResultsStats ValidateRange(VectorType &vec, TimePoint earliestPossibleTime) {
  using std::erase_if;

  using ObjType = std::remove_cvref_t<decltype(*std::declval<VectorType>().begin())>;

  static_assert(std::is_same_v<ObjType, MarketOrderBook> || std::is_same_v<ObjType, PublicTrade>);

  static constexpr std::string_view kObjName = std::is_same_v<ObjType, MarketOrderBook> ? "order book" : "trade";

  TradeRangeResultsStats stats;

  stats.nbSuccessful = static_cast<decltype(stats.nbSuccessful)>(vec.size());

  const auto nbInvalidObjects = erase_if(vec, [](const auto &obj) { return !obj.isValid(); });
  if (nbInvalidObjects != 0) {
    log::error("{} {}(s) with invalid data detected", nbInvalidObjects, kObjName);
  }

  const auto nbUnsortedObjectsRemoved = erase_if(vec, [&earliestPossibleTime](const auto &obj) {
    if (obj.time() < earliestPossibleTime) {
      return true;
    }
    earliestPossibleTime = obj.time();
    return false;
  });

  if (nbUnsortedObjectsRemoved != 0) {
    log::error("{} {}(s) are not in chronological order", nbUnsortedObjectsRemoved, kObjName);
  }

  if (!vec.empty()) {
    stats.timeWindow = TimeWindow(vec.front().time(), vec.back().time());
  }
  stats.nbError = nbInvalidObjects + nbUnsortedObjectsRemoved;
  stats.nbSuccessful -= stats.nbError;

  return stats;
}

}  // namespace

TradeRangeStats MarketTraderEngine::validateRange(MarketOrderBookVector &marketOrderBooks,
                                                  PublicTradeVector &publicTrades) {
  TimePoint earliestPossibleTime;
  if (_lastMarketOrderBook.market().isDefined()) {
    earliestPossibleTime = _lastMarketOrderBook.time();
  }

  TradeRangeStats tradeRangeStats;
  tradeRangeStats.marketOrderBookStats = ValidateRange(marketOrderBooks, earliestPossibleTime);
  tradeRangeStats.publicTradeStats = ValidateRange(publicTrades, earliestPossibleTime);

  return tradeRangeStats;
}

TradeRangeStats MarketTraderEngine::validateRange(MarketOrderBookVector &&marketOrderBooks,
                                                  PublicTradeVector &&publicTrades) {
  const TradeRangeStats tradeRangeStats = validateRange(marketOrderBooks, publicTrades);

  if (!marketOrderBooks.empty()) {
    _lastMarketOrderBook = std::move(marketOrderBooks.back());
  }

  return tradeRangeStats;
}

TradeRangeStats MarketTraderEngine::tradeRange(MarketOrderBookVector &&marketOrderBooks,
                                               PublicTradeVector &&publicTrades) {
  // errors set to 0 here as it is for unchecked launch
  TradeRangeStats tradeRangeStats{
      {TradeRangeResultsStats{TimeWindow{}, static_cast<int32_t>(marketOrderBooks.size()), 0}},
      TradeRangeResultsStats{TimeWindow{}, static_cast<int32_t>(publicTrades.size()), 0}};

  if (marketOrderBooks.empty()) {
    return tradeRangeStats;
  }

  const auto fromOrderBooksTime = marketOrderBooks.front().time();
  const auto toOrderBooksTime = marketOrderBooks.back().time();

  tradeRangeStats.marketOrderBookStats.timeWindow = TimeWindow(fromOrderBooksTime, toOrderBooksTime);

  if (!publicTrades.empty()) {
    tradeRangeStats.publicTradeStats.timeWindow = TimeWindow(publicTrades.front().time(), publicTrades.back().time());
  }

  log::info("[{}] at {} on {} replaying {} order books and {} trades", _marketTrader->name(),
            TimeToString(fromOrderBooksTime), _market, marketOrderBooks.size(), publicTrades.size());

  // Rolling window of data provided to underlying market trader with data up to latest market order book.
  MarketDataView marketDataView(marketOrderBooks.data(), publicTrades.data(),
                                publicTrades.data() + publicTrades.size());

  for (const MarketOrderBook &marketOrderBook : marketOrderBooks) {
    // First check opened orders status with new market order book data that may match some
    checkOpenedOrdersMatching(marketOrderBook);

    // We expect market data (order books and trades) to be sorted by time.
    // Advance the market data view iterator until including all data until last market order book time stamp.
    marketDataView.advanceUntil(marketOrderBook.time());

    // Call the user algorithm trading engine and retrieve its decision for next move
    const TraderCommand traderCommand = _marketTrader->trade(marketDataView);

    switch (traderCommand.type()) {
      case TraderCommand::Type::kWait:
        break;
      case TraderCommand::Type::kBuy: {
        const MonetaryAmount from = _marketTraderEngineState.computeBuyFrom(traderCommand);

        if (from != 0) {
          // Attempt to place an order without any available amount, do nothing instead
          buy(marketOrderBook, from, traderCommand.priceStrategy());
        }
        break;
      }
      case TraderCommand::Type::kSell: {
        const MonetaryAmount volume = _marketTraderEngineState.computeSellVolume(traderCommand);

        if (volume != 0) {
          // Attempt to place an order without any available amount, do nothing instead
          sell(marketOrderBook, volume, traderCommand.priceStrategy());
        }
        break;
      }
      case TraderCommand::Type::kUpdatePrice:
        updatePrice(marketOrderBook, traderCommand);
        break;
      case TraderCommand::Type::kCancel:
        cancelCommand(traderCommand.orderId());
        break;
      default:
        throw exception("Unsupported trader command {}", static_cast<int>(traderCommand.type()));
    }
  }

  _lastMarketOrderBook = std::move(marketOrderBooks.back());

  return tradeRangeStats;
}

MarketTradingResult MarketTraderEngine::finalizeAndComputeResult() {
  _marketTraderEngineState.cancelAllOpenedOrders();

  // How to compute gain / losses ?
  // Let's say we have {x1 XXX + y1 YYY} at the beginning, XXX-YYY being the market,
  // and {x2 XXX + y2 YYY} at the end.
  // The idea is that we speculate on the YYY currency on this market (we want to increase our YYY amount).
  // The formula used to compute gains / losses is the following:
  //   (y2 - y1) YYY + conversion((x2 - x1) XXX)->YYY +  at market price of the last market order book.

  MonetaryAmount quoteAmountDelta = _marketTraderEngineState.availableQuoteAmount() - _startAmountQuote;
  MonetaryAmount baseAmountDelta = _marketTraderEngineState.availableBaseAmount() - _startAmountBase;

  if (_lastMarketOrderBook.market().isNeutral()) {
    log::debug("Calling finalize on a market trader engine that has not been run");
  } else {
    auto [_, avgPrice] = _lastMarketOrderBook.avgPriceAndMatchedAmountTaker(baseAmountDelta.abs());

    quoteAmountDelta += baseAmountDelta.toNeutral() * avgPrice;
  }

  const auto closedOrdersSpan = _marketTraderEngineState.closedOrders();

  return {_marketTrader->name(), _startAmountBase, _startAmountQuote, quoteAmountDelta,
          ClosedOrderVector(closedOrdersSpan.begin(), closedOrdersSpan.end())};
}

void MarketTraderEngine::buy(const MarketOrderBook &marketOrderBook, MonetaryAmount from, PriceStrategy priceStrategy) {
  const auto ts = marketOrderBook.time();

  switch (priceStrategy) {
    case PriceStrategy::maker: {
      const MonetaryAmount price = marketOrderBook.highestBidPrice();
      const MonetaryAmount remainingVolume(from / price, _market.base());
      constexpr MonetaryAmount matchedVolume;

      _marketTraderEngineState.placeBuyOrder(_exchangeConfig, ts, remainingVolume, price, matchedVolume, from,
                                             schema::ExchangeTradeFeesConfig::FeeType::Maker);
      break;
    }
    case PriceStrategy::nibble: {
      const MonetaryAmount price = marketOrderBook.lowestAskPrice();
      const MonetaryAmount volume(from / price, _market.base());
      const MonetaryAmount matchedVolume = std::min(marketOrderBook.amountAtAskPrice(), volume);
      const MonetaryAmount remainingVolume = volume - matchedVolume;

      _marketTraderEngineState.placeBuyOrder(_exchangeConfig, ts, remainingVolume, price, matchedVolume, from,
                                             schema::ExchangeTradeFeesConfig::FeeType::Taker);
      break;
    }
    case PriceStrategy::taker: {
      const auto [totalMatchedAmount, avgPrice] = marketOrderBook.avgPriceAndMatchedAmountTaker(from);
      if (totalMatchedAmount != 0) {
        constexpr MonetaryAmount remainingVolume;

        _marketTraderEngineState.placeBuyOrder(_exchangeConfig, ts, remainingVolume, avgPrice, totalMatchedAmount, from,
                                               schema::ExchangeTradeFeesConfig::FeeType::Taker);
      }
      break;
    }
    default:
      throw exception("Unsupported price strategy {}", static_cast<int>(priceStrategy));
  }
}

void MarketTraderEngine::sell(const MarketOrderBook &marketOrderBook, MonetaryAmount volume,
                              PriceStrategy priceStrategy) {
  switch (priceStrategy) {
    case PriceStrategy::maker: {
      const MonetaryAmount price = marketOrderBook.lowestAskPrice();
      constexpr MonetaryAmount matchedVolume;

      _marketTraderEngineState.placeSellOrder(_exchangeConfig, marketOrderBook.time(), volume, price, matchedVolume,
                                              schema::ExchangeTradeFeesConfig::FeeType::Maker);
      break;
    }
    case PriceStrategy::nibble: {
      const MonetaryAmount price = marketOrderBook.highestBidPrice();
      const MonetaryAmount matchedVolume = std::min(marketOrderBook.amountAtBidPrice(), volume);

      _marketTraderEngineState.placeSellOrder(_exchangeConfig, marketOrderBook.time(), volume - matchedVolume, price,
                                              matchedVolume, schema::ExchangeTradeFeesConfig::FeeType::Taker);
      break;
    }
    case PriceStrategy::taker: {
      const auto [totalMatchedAmount, avgPrice] = marketOrderBook.avgPriceAndMatchedAmountTaker(volume);

      if (totalMatchedAmount != 0) {
        constexpr MonetaryAmount remainingVolume;

        _marketTraderEngineState.placeSellOrder(_exchangeConfig, marketOrderBook.time(), remainingVolume, avgPrice,
                                                totalMatchedAmount, schema::ExchangeTradeFeesConfig::FeeType::Taker);
      }
      break;
    }
    default:
      throw exception("Unsupported price strategy {}", static_cast<int>(priceStrategy));
  }
}

void MarketTraderEngine::updatePrice(const MarketOrderBook &marketOrderBook, TraderCommand traderCommand) {
  const auto orderIdIt = _marketTraderEngineState.findOpenedOrder(traderCommand.orderId());
  MonetaryAmount remainingAmount = orderIdIt->remainingVolume();
  TradeSide tradeSide = orderIdIt->side();
  MonetaryAmount price = orderIdIt->price();

  _marketTraderEngineState.cancelOpenedOrder(traderCommand.orderId());

  switch (tradeSide) {
    case TradeSide::kBuy:
      buy(marketOrderBook, remainingAmount.toNeutral() * price, traderCommand.priceStrategy());
      break;
    case TradeSide::kSell:
      sell(marketOrderBook, remainingAmount, traderCommand.priceStrategy());
      break;
    default:
      throw exception("Unsupported trade side");
  }
}

void MarketTraderEngine::cancelCommand(int32_t orderId) {
  if (orderId == TraderCommand::kAllOrdersId) {
    _marketTraderEngineState.cancelAllOpenedOrders();
  } else {
    _marketTraderEngineState.cancelOpenedOrder(orderId);
  }
}

void MarketTraderEngine::checkOpenedOrdersMatching(const MarketOrderBook &marketOrderBook) {
  _newlyClosedOrders.clear();
  for (const OpenedOrder &openedOrder : _marketTraderEngineState.openedOrders()) {
    const auto [newMatchedVolume, avgPrice] = marketOrderBook.avgPriceAndMatchedVolume(
        openedOrder.side(), openedOrder.remainingVolume(), openedOrder.price());
    if (newMatchedVolume == 0) {
      continue;
    }

    _marketTraderEngineState.countMatchedPart(_exchangeConfig, openedOrder, avgPrice, newMatchedVolume,
                                              marketOrderBook.time());

    if (newMatchedVolume == openedOrder.remainingVolume()) {
      _newlyClosedOrders.push_back(openedOrder);
    } else {
      _marketTraderEngineState.adjustOpenedOrderRemainingVolume(openedOrder, newMatchedVolume);
    }
  }

  _marketTraderEngineState.eraseClosedOpenedOrders(_newlyClosedOrders);
}

}  // namespace cct
