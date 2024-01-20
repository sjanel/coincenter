#include "withdrawsordepositsconstraints.hpp"

#include <string_view>
#include <utility>

#include "baseconstraints.hpp"
#include "currencycode.hpp"
#include "timedef.hpp"

namespace cct {
WithdrawsOrDepositsConstraints::WithdrawsOrDepositsConstraints(CurrencyCode currencyCode, Duration minAge,
                                                               Duration maxAge, IdSet &&idSet)
    : _idSet(std::move(idSet)), _currencyCode(currencyCode) {
  if (!_idSet.empty()) {
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kId);
  }
  const auto now = Clock::now();
  if (minAge != kUndefinedDuration) {
    _timeBefore = now - minAge;
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kReceivedBefore);
  }
  if (maxAge != kUndefinedDuration) {
    _timeAfter = now - maxAge;
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kReceivedAfter);
  }
  if (!_currencyCode.isNeutral()) {
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kCur);
  }
}

WithdrawsOrDepositsConstraints::WithdrawsOrDepositsConstraints(CurrencyCode currencyCode, std::string_view id)
    : _currencyCode(currencyCode) {
  if (!id.empty()) {
    _idSet.emplace(id);
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kId);
  }
  if (!_currencyCode.isNeutral()) {
    _currencyIdTimeConstraintsBmp.set(CurrencyIdTimeConstraintsBmp::ConstraintType::kCur);
  }
}

}  // namespace cct