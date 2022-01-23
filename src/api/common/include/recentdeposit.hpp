#pragma once

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timehelpers.hpp"

namespace cct {
class RecentDeposit {
 public:
  using RecentDepositVector = SmallVector<RecentDeposit, 4>;

  RecentDeposit(MonetaryAmount amount, TimePoint timepoint) : _amount(amount), _timepoint(timepoint) {}

  MonetaryAmount amount() const { return _amount; }
  TimePoint timePoint() const { return _timepoint; }

  /// Select the RecentDeposit among given ones which is the closest to 'this' object.
  /// It may reorder the given vector but will not modify objects themselves.
  const RecentDeposit *selectClosestRecentDeposit(RecentDepositVector &recentDeposits) const;

  string str() const;

 private:
  MonetaryAmount _amount;
  TimePoint _timepoint;
};
}  // namespace cct