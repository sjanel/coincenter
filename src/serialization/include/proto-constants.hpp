#pragma once

#include <cstdint>
#include <string_view>

namespace cct {

enum class ProtobufObject : int8_t { kMarketOrderBook, kTrade };

/// Compression format of the serialized protobuf files. Readers detect it automatically from the file content,
/// so files written in any format stay readable.
enum class ProtobufCompression : int8_t { kGzip, kZstd };

/// Format used for new files.
inline constexpr ProtobufCompression kDefaultProtobufCompression = ProtobufCompression::kZstd;

inline constexpr std::string_view kBinProtobufExtension = ".binpb";

inline constexpr std::string_view kSubPathMarketOrderBooks = "order-books";
inline constexpr std::string_view kSubPathTrades = "trades";

}  // namespace cct