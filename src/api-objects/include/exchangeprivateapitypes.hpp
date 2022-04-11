#pragma once

#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "order.hpp"

namespace cct {
using Orders = vector<Order>;
using OrdersSet = FlatSet<Order>;
}  // namespace cct