#pragma once

#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "closed-order.hpp"
#include "deposit.hpp"
#include "opened-order.hpp"
#include "tradedamounts.hpp"
#include "withdraw.hpp"

namespace cct {

using Deposits = vector<Deposit>;
using DepositsSet = FlatSet<Deposit>;

using Withdraws = vector<Withdraw>;
using WithdrawsSet = FlatSet<Withdraw>;

using ClosedOrderVector = vector<ClosedOrder>;
using ClosedOrderSet = FlatSet<ClosedOrder>;

using OpenedOrderVector = vector<OpenedOrder>;
using OpenedOrderSet = FlatSet<OpenedOrder>;

using TradedAmountsVector = vector<TradedAmounts>;

struct TradedAmountsVectorWithFinalAmount {
  TradedAmountsVector tradedAmountsVector;
  MonetaryAmount finalAmount;
};

}  // namespace cct
