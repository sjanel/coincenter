#pragma once

#include <map>
#include <string_view>

#include "cct_string.hpp"
#include "exchangeinfo.hpp"

namespace cct {
using ExchangeInfoMap = std::map<string, ExchangeInfo, std::less<>>;

ExchangeInfoMap ComputeExchangeInfoMap(std::string_view dataDir);
}  // namespace cct