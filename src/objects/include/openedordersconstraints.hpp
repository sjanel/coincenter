#pragma once

#include <chrono>

#include "cct_string.hpp"
#include "currencycode.hpp"
#include "market.hpp"

namespace cct {
class OpenedOrdersConstraints {
 public:
  using TimePoint = std::chrono::system_clock::time_point;
  using Duration = std::chrono::system_clock::duration;

  /// Build OpenedOrderConstraints based on given currency(ies) and placed after time
  explicit OpenedOrdersConstraints(CurrencyCode cur1 = CurrencyCode(), CurrencyCode cur2 = CurrencyCode(),
                                   Duration minAge = Duration(), Duration maxAge = Duration());

  TimePoint placedAfter() const { return _placedAfter; }

  bool isPlacedTimeDefined() const { return _placedAfter != TimePoint::min(); }

  bool validatePlacedTime(TimePoint t) const { return t >= _placedAfter && t <= _placedBefore; }

  bool validateCur(CurrencyCode cur1, CurrencyCode cur2) const {
    if (_cur1 == CurrencyCode() && _cur2 == CurrencyCode()) {
      return true;
    }
    if (_cur1 == CurrencyCode()) {
      return _cur2 == cur1 || _cur2 == cur2;
    }
    if (_cur2 == CurrencyCode()) {
      return _cur1 == cur1 || _cur1 == cur2;
    }
    return (_cur1 == cur1 && _cur2 == cur2) || (_cur1 == cur2 && _cur2 == cur1);
  }

  bool isCur1Defined() const { return !_cur1.isNeutral(); }
  bool isCur2Defined() const { return !_cur2.isNeutral(); }
  bool isMarketDefined() const { return isCur1Defined() && isCur2Defined(); }

  Market market() const { return Market(_cur1, _cur2); }

  std::string_view curStr1() const { return _cur1.str(); }
  std::string_view curStr2() const { return _cur2.str(); }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  string str() const;

 private:
  TimePoint _placedBefore;
  TimePoint _placedAfter;
  CurrencyCode _cur1;
  CurrencyCode _cur2;
};
}  // namespace cct