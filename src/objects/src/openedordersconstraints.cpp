#include "openedordersconstraints.hpp"

#include "timestring.hpp"

namespace cct {

OpenedOrdersConstraints::OpenedOrdersConstraints(CurrencyCode cur1, CurrencyCode cur2, Duration minAge, Duration maxAge)
    : _placedBefore(TimePoint::max()), _placedAfter(TimePoint::min()), _cur1(cur1), _cur2(cur2) {
  auto now = std::chrono::system_clock::now();
  if (minAge != Duration()) {
    _placedBefore = now - minAge;
  }
  if (maxAge != Duration()) {
    _placedAfter = now - maxAge;
  }
}

string OpenedOrdersConstraints::str() const {
  string ret;
  if (isCur1Defined()) {
    ret.append(_cur1.str());
  }
  if (isCur2Defined()) {
    ret.push_back('-');
    ret.append(_cur2.str());
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
  return ret;
}
}  // namespace cct