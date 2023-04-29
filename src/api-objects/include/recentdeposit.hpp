#pragma once

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class RecentDeposit {
 public:
  using RecentDepositVector = SmallVector<RecentDeposit, 4>;

  RecentDeposit(MonetaryAmount amount, TimePoint timePoint) : _amount(amount), _timePoint(timePoint) {}

  MonetaryAmount amount() const { return _amount; }
  TimePoint timePoint() const { return _timePoint; }

  /// Select the RecentDeposit among given ones which is the closest to 'this' object.
  /// It may reorder the given vector but will not modify objects themselves.
  /// Returns nullptr if no matching deposit has been found
  const RecentDeposit *selectClosestRecentDeposit(RecentDepositVector &recentDeposits) const;

  string str() const;

 private:
  MonetaryAmount _amount;
  TimePoint _timePoint;
};
}  // namespace cct