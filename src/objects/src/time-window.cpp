#include "time-window.hpp"

#include <algorithm>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {

TimeWindow::TimeWindow(std::string_view timeWindowStr) {
  auto openingBracketPos = timeWindowStr.find('[');
  if (openingBracketPos == std::string_view::npos) {
    throw invalid_argument("Invalid time window - missing opening bracket");
  }
  auto arrowPos = timeWindowStr.find(kArrow, openingBracketPos);
  if (arrowPos == std::string_view::npos) {
    throw invalid_argument("Invalid time window - missing arrow");
  }
  auto closingBracketPos = timeWindowStr.find(')', arrowPos);
  if (closingBracketPos == std::string_view::npos) {
    throw invalid_argument("Invalid time window - missing closing bracket");
  }

  _from = StringToTimeISO8601UTC(timeWindowStr.data() + openingBracketPos + 1, timeWindowStr.data() + arrowPos);
  _to =
      StringToTimeISO8601UTC(timeWindowStr.data() + arrowPos + kArrow.size(), timeWindowStr.data() + closingBracketPos);
}

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
  string ret(kTimeWindowLen, '\0');
  appendTo(ret.data());
  return ret;
}

char *TimeWindow::appendTo(char *buf) const {
  *buf++ = '[';
  buf = std::ranges::copy(TimeToString(from(), kTimeFormat), buf).out;
  buf = std::ranges::copy(kArrow, buf).out;
  buf = std::ranges::copy(TimeToString(to(), kTimeFormat), buf).out;
  *buf++ = ')';
  return buf;
}

}  // namespace cct