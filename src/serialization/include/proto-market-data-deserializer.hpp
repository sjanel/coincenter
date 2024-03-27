#pragma once

#include <string_view>

#include "abstract-market-data-deserializer.hpp"
#include "market-order-book-timed-data.pb.h"
#include "market-order-book-vector.hpp"
#include "market-vector.hpp"
#include "market.hpp"
#include "proto-deserializer.hpp"
#include "proto-market-order-book.hpp"
#include "proto-public-trade.hpp"
#include "public-trade-vector.hpp"
#include "time-window.hpp"
#include "trade-data.pb.h"

namespace cct {

class ProtoMarketDataDeserializer : public AbstractMarketDataDeserializer {
 public:
  ProtoMarketDataDeserializer(std::string_view dataDir, std::string_view exchangeName);

  MarketVector pullMarketOrderBooksMarkets(TimeWindow timeWindow) override;

  MarketVector pullTradeMarkets(TimeWindow timeWindow) override;

  MarketOrderBookVector pullMarketOrderBooks(Market market, TimeWindow timeWindow) override;

  PublicTradeVector pullTrades(Market market, TimeWindow timeWindow) override;

 private:
  ProtobufObjectsDeserializer<::objects::MarketOrderBookTimedData, MarketOrderBookConverter>
      _marketOrderBookDataGateway;
  ProtobufObjectsDeserializer<::objects::TradeData, TradeDataToPublicTradeConverter> _tradeDataGateway;
};
}  // namespace cct