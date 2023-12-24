#include "time-window.hpp"

#include "timestring.hpp"

namespace cct {
string TimeWindow::str() const {
  string ret;
  ret.push_back('[');
  ret.append(ToString(from(), kTimeYearToSecondSpaceSeparatedFormat));
  ret.append(" -> ");
  ret.append(ToString(to(), kTimeYearToSecondSpaceSeparatedFormat));
  ret.push_back(')');
  return ret;
}
}  // namespace cct