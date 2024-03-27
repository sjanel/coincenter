#pragma once

#include <span>
#include <string_view>

#include "abstract-market-data-serializer.hpp"
#include "marketorderbook.hpp"
#include "publictrade.hpp"

namespace cct {
/// Implementation of a market data serializer that does nothing.
/// Useful if coincenter is not compiled with protobuf support.
class DummyMarketDataSerializer : public AbstractMarketDataSerializer {
 public:
  DummyMarketDataSerializer(std::string_view dataDir, std::string_view exchangeName);

  void push(const MarketOrderBook &marketOrderBook) override;

  void push(Market market, std::span<const PublicTrade> publicTrades) override;
};
}  // namespace cct