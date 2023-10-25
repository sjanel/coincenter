#include "ordersconstraints.hpp"

#include <utility>

#include "currencycode.hpp"
#include "timedef.hpp"

namespace cct {

OrdersConstraints::OrdersConstraints(CurrencyCode cur1, CurrencyCode cur2, Duration minAge, Duration maxAge,
                                     OrderIdSet &&ordersIdSet)
    : _ordersIdSet(std::move(ordersIdSet)),
      _placedBefore(TimePoint::max()),
      _placedAfter(TimePoint::min()),
      _cur1(cur1),
      _cur2(cur2) {
  if (!_ordersIdSet.empty()) {
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kId);
  }
  auto now = Clock::now();
  if (minAge != Duration()) {
    _placedBefore = now - minAge;
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kPlacedBefore);
  }
  if (maxAge != Duration()) {
    _placedAfter = now - maxAge;
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kPlacedAfter);
  }
  if (!_cur1.isNeutral()) {
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kCur1);
  }
  if (!_cur2.isNeutral()) {
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kCur2);
  }
}

}  // namespace cct