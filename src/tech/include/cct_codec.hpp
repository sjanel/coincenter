#pragma once

#include <span>
#include <string_view>

#include "cct_string.hpp"

namespace cct {

string BinToHex(std::span<const unsigned char> bindata);

string B64Encode(std::string_view bindata);

string B64Decode(std::string_view ascdata);
}  // namespace cct