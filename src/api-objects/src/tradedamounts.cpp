#include "tradedamounts.hpp"

namespace cct {

string TradedAmounts::str() const {
  string ret;
  ret.append(tradedFrom.str());
  ret.append(" -> ");
  ret.append(tradedTo.str());
  return ret;
}

std::ostream &operator<<(std::ostream &os, const TradedAmounts &tradedAmounts) {
  os << tradedAmounts.tradedFrom << " -> " << tradedAmounts.tradedTo;
  return os;
}

}  // namespace cct