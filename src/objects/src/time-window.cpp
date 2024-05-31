#include "time-window.hpp"

#include "timestring.hpp"

namespace cct {
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