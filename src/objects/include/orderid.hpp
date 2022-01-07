#pragma once

#include <span>

#include "cct_string.hpp"

namespace cct {
using OrderId = string;
using OrderIds = std::span<const OrderId>;
}  // namespace cct