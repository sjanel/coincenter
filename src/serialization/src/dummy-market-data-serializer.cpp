#include "dummy-market-data-serializer.hpp"

#include <span>
#include <string_view>

#include "marketorderbook.hpp"
#include "publictrade.hpp"

namespace cct {

DummyMarketDataSerializer::DummyMarketDataSerializer([[maybe_unused]] std::string_view dataDir,
                                                     [[maybe_unused]] std::string_view exchangeName) {}

void DummyMarketDataSerializer::push([[maybe_unused]] const MarketOrderBook &marketOrderBook) {}

void DummyMarketDataSerializer::push([[maybe_unused]] Market market,
                                     [[maybe_unused]] std::span<const PublicTrade> publicTrades) {}

}  // namespace cct