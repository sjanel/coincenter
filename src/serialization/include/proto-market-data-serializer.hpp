#pragma once

#include <span>
#include <string_view>

#include "abstract-market-data-serializer.hpp"
#include "market-order-book-timed-data.pb.h"
#include "marketorderbook.hpp"
#include "proto-serializer.hpp"
#include "publictrade.hpp"
#include "trade-data.pb.h"

namespace cct {

/// This class is responsible of managing the periodic writes to disk of timed market data, for a given exchange.
/// This class is not thread safe
class ProtoMarketDataSerializer : public AbstractMarketDataSerializer {
 public:
  ProtoMarketDataSerializer(std::string_view dataDir, std::string_view exchangeName);

  void push(const MarketOrderBook &marketOrderBook) override;

  void push(Market market, std::span<const PublicTrade> publicTrades) override;

 private:
  struct TradeDataComp {
    bool operator()(const ::objects::TradeData &lhs, const ::objects::TradeData &rhs) const;
  };

  struct TradeDataEqual {
    bool operator()(const ::objects::TradeData &lhs, const ::objects::TradeData &rhs) const;
  };

  ProtobufObjectsSerializer<::objects::MarketOrderBookTimedData> _marketOrderBookAccumulator;
  ProtobufObjectsSerializer<::objects::TradeData, TradeDataComp, TradeDataEqual> _tradesAccumulator;
};

}  // namespace cct