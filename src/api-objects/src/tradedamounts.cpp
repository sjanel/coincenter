#include "tradedamounts.hpp"

#include <ostream>

#include "cct_string.hpp"

namespace cct {

string TradedAmounts::str() const {
  string ret = from.str();
  ret.append(" -> ");
  to.appendStrTo(ret);
  return ret;
}

std::ostream &operator<<(std::ostream &os, const TradedAmounts &tradedAmounts) {
  return os << tradedAmounts.from << " -> " << tradedAmounts.to;
}

}  // namespace cct