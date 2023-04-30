#pragma once

#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "deposit.hpp"
#include "order.hpp"
#include "tradedamounts.hpp"
#include "withdraw.hpp"

namespace cct {
using Deposits = vector<Deposit>;
using DepositsSet = FlatSet<Deposit>;

using Withdraws = vector<Withdraw>;
using WithdrawsSet = FlatSet<Withdraw>;

using Orders = vector<Order>;
using OrdersSet = FlatSet<Order>;

using TradedAmountsVector = vector<TradedAmounts>;

struct TradedAmountsVectorWithFinalAmount {
  TradedAmountsVector tradedAmountsVector;
  MonetaryAmount finalAmount;
};
}  // namespace cct
