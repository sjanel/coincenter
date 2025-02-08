#pragma once

#include <cstdint>
#include <string_view>

namespace cct {

enum class ProtobufObject : int8_t { kMarketOrderBook, kTrade };

inline constexpr std::string_view kBinProtobufExtension = ".binpb";

inline constexpr std::string_view kSubPathMarketOrderBooks = "order-books";
inline constexpr std::string_view kSubPathTrades = "trades";

}  // namespace cct