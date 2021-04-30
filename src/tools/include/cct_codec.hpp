#pragma once

#include <string>
#include <string_view>

namespace cct {

std::string BinToHex(const unsigned char* in, int size);

std::string B64Encode(std::string_view bindata);

std::string B64Decode(std::string_view ascdata);
}  // namespace cct