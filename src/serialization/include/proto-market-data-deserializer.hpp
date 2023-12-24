#pragma once

#include <string_view>

#include "abstract-market-data-deserializer.hpp"
#include "market-order-book-vector.hpp"
#include "market-order-book.pb.h"
#include "market-timestamp-set.hpp"
#include "market.hpp"
#include "proto-deserializer.hpp"
#include "proto-market-order-book-converter.hpp"
#include "proto-public-trade-converter.hpp"
#include "public-trade-vector.hpp"
#include "public-trade.pb.h"
#include "time-window.hpp"

namespace cct {

class ProtoMarketDataDeserializer : public AbstractMarketDataDeserializer {
 public:
  ProtoMarketDataDeserializer(std::string_view dataDir, std::string_view exchangeName);

  MarketTimestampSet pullMarketOrderBooksMarkets(TimeWindow timeWindow) override;

  MarketTimestampSet pullTradeMarkets(TimeWindow timeWindow) override;

  MarketOrderBookVector pullMarketOrderBooks(Market market, TimeWindow timeWindow) override;

  PublicTradeVector pullTrades(Market market, TimeWindow timeWindow) override;

 private:
  ProtobufObjectsDeserializer<::proto::MarketOrderBook, MarketOrderBookConverter> _marketOrderBookDeserializer;
  ProtobufObjectsDeserializer<::proto::PublicTrade, PublicTradeConverter> _publicTradeDeserializer;
};

}  // namespace cct