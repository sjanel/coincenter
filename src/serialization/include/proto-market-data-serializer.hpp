#pragma once

#include <span>
#include <string_view>

#include "abstract-market-data-serializer.hpp"
#include "market-order-book.pb.h"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "proto-public-trade-compare.hpp"
#include "proto-serializer.hpp"
#include "public-trade.pb.h"
#include "publictrade.hpp"

namespace cct {

class MarketOrderBook;

/// This class is responsible of managing the periodic writes to disk of timed market data, for a given exchange.
/// This class is not thread safe
class ProtoMarketDataSerializer : public AbstractMarketDataSerializer {
 public:
  ProtoMarketDataSerializer(std::string_view dataDir, const MarketTimestampSets &lastWrittenObjectsMarketTimestamp,
                            std::string_view exchangeName);

  void push(const MarketOrderBook &marketOrderBook) override;

  void push(Market market, std::span<const PublicTrade> publicTrades) override;

 private:
  ProtobufObjectsSerializer<::proto::MarketOrderBook> _marketOrderBookSerializer;
  ProtobufObjectsSerializer<::proto::PublicTrade, ProtoPublicTradeComp, ProtoPublicTradeEqual> _tradesSerializer;
};

}  // namespace cct