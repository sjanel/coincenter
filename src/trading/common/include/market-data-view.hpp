#pragma once

#include <cstddef>
#include <span>

#include "marketorderbook.hpp"
#include "publictrade.hpp"
#include "timedef.hpp"

namespace cct {

/// A class providing a view to current and historical market data for the market trader.
class MarketDataView {
 public:
  /// Get a reference to last (current for this turn) market order book
  const MarketOrderBook &currentMarketOrderBook() const { return _pOrderBooks[_currentOrderBookEndPos - 1U]; }

  /// Get a span of all historical market order books since the start of the market trader engine (including current /
  /// last one)
  std::span<const MarketOrderBook> pastMarketOrderBooks() const { return {_pOrderBooks, _currentOrderBookEndPos}; }

  /// Get a span of all new public trades that occurred before last (current for this turn) market order book that have
  /// not been seen before.
  std::span<const PublicTrade> currentPublicTrades() const { return {_pCurrentTradesBeg, _pCurrentTradesEnd}; }

  /// Get a span of all public trades since the start of the market trader engine (including current / last ones).
  std::span<const PublicTrade> pastPublicTrades() const { return {_pPublicTradesBeg, _pCurrentTradesEnd}; }

 private:
  friend class MarketTraderEngine;

  MarketDataView(const MarketOrderBook *pOrderBooks, const PublicTrade *pPublicTradesBeg,
                 const PublicTrade *pPublicTradesEnd) noexcept;

  void advanceUntil(TimePoint marketOrderBookTs);

  const MarketOrderBook *_pOrderBooks;
  const PublicTrade *_pPublicTradesBeg;
  const PublicTrade *_pPublicTradesEnd;

  const PublicTrade *_pCurrentTradesBeg;
  const PublicTrade *_pCurrentTradesEnd;
  std::size_t _currentOrderBookEndPos{};
};

}  // namespace cct