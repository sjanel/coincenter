#include "market-data-view.hpp"

#include <algorithm>

#include "publictrade.hpp"

namespace cct {

MarketDataView::MarketDataView(const MarketOrderBook *pOrderBooks, const PublicTrade *pPublicTradesBeg,
                               const PublicTrade *pPublicTradesEnd) noexcept
    : _pOrderBooks(pOrderBooks),
      _pPublicTradesBeg(pPublicTradesBeg),
      _pPublicTradesEnd(pPublicTradesEnd),
      _pCurrentTradesBeg(pPublicTradesBeg),
      _pCurrentTradesEnd(pPublicTradesEnd) {}

void MarketDataView::advanceUntil(TimePoint marketOrderBookTs) {
  // Advance the public trades iterator until we reach one that occurred after our current market order book
  _pCurrentTradesBeg = _pCurrentTradesEnd;
  _pCurrentTradesEnd = std::partition_point(
      _pCurrentTradesBeg, _pPublicTradesEnd,
      [marketOrderBookTs](const auto &publicTrade) { return publicTrade.time() < marketOrderBookTs; });

  ++_currentOrderBookEndPos;
}

}  // namespace cct