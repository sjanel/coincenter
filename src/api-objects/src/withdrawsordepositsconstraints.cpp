#include "withdrawsordepositsconstraints.hpp"

namespace cct {
WithdrawsOrDepositsConstraints::WithdrawsOrDepositsConstraints(CurrencyCode currencyCode, Duration minAge,
                                                               Duration maxAge, IdSet &&idSet)
    : _idSet(std::move(idSet)),
      _timeBefore(TimePoint::max()),
      _timeAfter(TimePoint::min()),
      _currencyCode(currencyCode) {
  if (!_idSet.empty()) {
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kId);
  }
  auto now = Clock::now();
  if (minAge != Duration()) {
    _timeBefore = now - minAge;
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kReceivedBefore);
  }
  if (maxAge != Duration()) {
    _timeAfter = now - maxAge;
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kReceivedAfter);
  }
  if (!_currencyCode.isNeutral()) {
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kCur);
  }
}

}  // namespace cct