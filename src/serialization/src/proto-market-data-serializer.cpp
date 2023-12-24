#include "proto-market-data-serializer.hpp"

#include <span>
#include <string_view>

#include "cct_log.hpp"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "marketorderbook.hpp"
#include "proto-constants.hpp"
#include "proto-market-order-book-converter.hpp"
#include "proto-public-trade-converter.hpp"
#include "publictrade.hpp"
#include "serialization-tools.hpp"

namespace cct {

namespace {
constexpr auto kNbMarketOrderBookObjectsInMemory = 1000;
constexpr auto kNbTradeObjectsInMemory = 25000;
}  // namespace

ProtoMarketDataSerializer::ProtoMarketDataSerializer(std::string_view dataDir,
                                                     const MarketTimestampSets& lastWrittenObjectsMarketTimestamp,
                                                     std::string_view exchangeName)
    : _marketOrderBookSerializer(ComputeProtoSubPath(dataDir, exchangeName, kSubPathMarketOrderBooks),
                                 lastWrittenObjectsMarketTimestamp.orderBooksMarkets,
                                 kNbMarketOrderBookObjectsInMemory),
      _tradesSerializer(ComputeProtoSubPath(dataDir, exchangeName, kSubPathTrades),
                        lastWrittenObjectsMarketTimestamp.tradesMarkets, kNbTradeObjectsInMemory) {}

void ProtoMarketDataSerializer::push(const MarketOrderBook& marketOrderBook) {
  if (!marketOrderBook.isValid()) {
    log::error("Do not serialize invalid market order book");
    return;
  }
  _marketOrderBookSerializer.push(marketOrderBook.market(), ConvertMarketOrderBookToProto(marketOrderBook));
}

void ProtoMarketDataSerializer::push(Market market, std::span<const PublicTrade> publicTrades) {
  for (const auto& publicTrade : publicTrades) {
    if (!publicTrade.isValid()) {
      log::error("Do not serialize invalid public trade");
      continue;
    }
    _tradesSerializer.push(market, ConvertPublicTradeToProto(publicTrade));
  }
}

}  // namespace cct