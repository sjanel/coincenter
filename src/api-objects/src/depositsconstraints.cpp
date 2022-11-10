#include "depositsconstraints.hpp"

namespace cct {
DepositsConstraints::DepositsConstraints(CurrencyCode currencyCode, Duration minAge, Duration maxAge,
                                         DepositIdSet &&depositIdSet)
    : _depositIdSet(std::move(depositIdSet)),
      _receivedBefore(TimePoint::max()),
      _receivedAfter(TimePoint::min()),
      _currencyCode(currencyCode) {
  if (!_depositIdSet.empty()) {
    _depositsConstraintsBmp.set(DepositsConstraintsBitmap::ConstraintType::kId);
  }
  auto now = Clock::now();
  if (minAge != Duration()) {
    _receivedBefore = now - minAge;
    _depositsConstraintsBmp.set(DepositsConstraintsBitmap::ConstraintType::kReceivedBefore);
  }
  if (maxAge != Duration()) {
    _receivedAfter = now - maxAge;
    _depositsConstraintsBmp.set(DepositsConstraintsBitmap::ConstraintType::kReceivedAfter);
  }
  if (!_currencyCode.isNeutral()) {
    _depositsConstraintsBmp.set(DepositsConstraintsBitmap::ConstraintType::kCur);
  }
}

}  // namespace cct