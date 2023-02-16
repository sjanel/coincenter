#pragma once

#include <span>
#include <string_view>

#include "cct_string.hpp"

namespace cct {
using OrderId = string;
using OrderIdView = std::string_view;
using OrderIds = std::span<const OrderId>;
}  // namespace cct