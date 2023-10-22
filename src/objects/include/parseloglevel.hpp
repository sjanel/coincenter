#pragma once

#include <cstdint>
#include <string_view>

namespace cct {
int8_t LogPosFromLogStr(std::string_view logStr);
}