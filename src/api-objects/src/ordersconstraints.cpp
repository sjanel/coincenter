#include "ordersconstraints.hpp"

#include "timestring.hpp"

namespace cct {

OrdersConstraints::OrdersConstraints(CurrencyCode cur1, CurrencyCode cur2, Duration minAge, Duration maxAge,
                                     OrderIdSet &&ordersIdSet)
    : _ordersIdSet(std::move(ordersIdSet)),
      _placedBefore(TimePoint::max()),
      _placedAfter(TimePoint::min()),
      _cur1(cur1),
      _cur2(cur2) {
  if (!_ordersIdSet.empty()) {
    _orderConstraintsBitmap.set(OrderConstraintsBitmap::ConstraintType::kOrderId);
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

string OrdersConstraints::str() const {
  string ret;
  if (isCur1Defined()) {
    _cur1.appendStr(ret);
  }
  if (isCur2Defined()) {
    ret.push_back('-');
    _cur2.appendStr(ret);
  }
  if (ret.empty()) {
    ret.append("any");
  }
  ret.append(" currencies");
  if (_placedBefore != TimePoint::max()) {
    ret.append(" before ");
    ret.append(ToString(_placedBefore));
  }
  if (_placedAfter != TimePoint::min()) {
    ret.append(" after ");
    ret.append(ToString(_placedAfter));
  }
  if (isOrderIdDefined()) {
    ret.append(" matching Ids [");
    for (const OrderId &orderId : _ordersIdSet) {
      if (ret.back() != '[') {
        ret.push_back(',');
      }
      ret.append(orderId);
    }
    ret.push_back(']');
  }
  return ret;
}
}  // namespace cct