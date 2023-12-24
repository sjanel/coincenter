#pragma once

#include <cstdint>
#include <string_view>

namespace cct {

enum class ProtobufObject : int8_t { kMarketOrderBook, kTrade };

static constexpr std::string_view kBinProtobufExtension = ".binpb";

static constexpr std::string_view kSubPathMarketOrderBooks = "order-books";
static constexpr std::string_view kSubPathTrades = "trades";

}  // namespace cct