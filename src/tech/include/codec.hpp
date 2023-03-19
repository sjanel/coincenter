#pragma once

#include <span>

#include "cct_string.hpp"

namespace cct {

// const char * arguments are deleted because it would construct into a span including the unwanted null
// terminating character. Use span directly, or string / string_view instead.

[[nodiscard]] string BinToHex(std::span<const unsigned char> binData);
string BinToHex(const char *) = delete;

[[nodiscard]] string B64Encode(std::span<const char> binData);
string B64Encode(const char *) = delete;

[[nodiscard]] string B64Decode(std::span<const char> ascData);
string B64Decode(const char *) = delete;
}  // namespace cct