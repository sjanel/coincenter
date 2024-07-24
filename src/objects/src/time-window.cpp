#include "time-window.hpp"

#include <algorithm>

#include "cct_string.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {

TimeWindow TimeWindow::aggregateMinMax(TimeWindow rhs) const {
  TimeWindow ret{_from, std::max(_to, rhs._to)};
  if (_from == TimePoint{}) {
    ret._from = rhs._from;
  } else if (rhs._from != TimePoint{}) {
    ret._from = std::min(_from, rhs._from);
  }

  return ret;
}

string TimeWindow::str() const {
  string ret;
  ret.push_back('[');
  ret.append(TimeToString(from(), kTimeYearToSecondSpaceSeparatedFormat));
  ret.append(" -> ");
  ret.append(TimeToString(to(), kTimeYearToSecondSpaceSeparatedFormat));
  ret.push_back(')');
  return ret;
}

}  // namespace cct