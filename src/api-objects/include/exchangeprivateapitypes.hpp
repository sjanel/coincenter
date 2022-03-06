#pragma once

#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "order.hpp"
#include "tradedamounts.hpp"

namespace cct {
using Orders = vector<Order>;
using OrdersSet = FlatSet<Order>;
using TradedAmountsVector = vector<TradedAmounts>;

struct TradedAmountsVectorWithFinalAmount {
  TradedAmountsVector tradedAmountsVector;
  MonetaryAmount finalAmount;
};
}  // namespace cct
