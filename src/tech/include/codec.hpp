#pragma once

#include <span>

#include "cct_string.hpp"

namespace cct {

// const char * arguments are deleted because it would construct into a span including the unwanted null
// terminating character. Use span directly, or string / string_view instead.

string BinToHex(std::span<const unsigned char> bindata);
string BinToHex(const char *) = delete;

string B64Encode(std::span<const char> bindata);
string B64Encode(const char *) = delete;

string B64Decode(std::span<const char> ascdata);
string B64Decode(const char *) = delete;
}  // namespace cct