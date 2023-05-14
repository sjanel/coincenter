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

/// This function converts the given input string to a URL encoded string.
/// All input characters that are not a-z, A-Z, 0-9, '-', '.', '_' or '~' are converted to their "URL escaped" version
/// (%NN where NN is a two-digit hexadecimal number).
[[nodiscard]] string URLEncode(std::span<const char> ascData);
string URLEncode(const char *) = delete;

}  // namespace cct