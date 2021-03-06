#include "tradedamounts.hpp"

namespace cct {
TradedAmounts TradedAmounts::operator+(const TradedAmounts &o) const {
  return TradedAmounts(tradedFrom + o.tradedFrom, tradedTo + o.tradedTo);
}

string TradedAmounts::str() const {
  string ret;
  ret.append(tradedFrom.str());
  ret.append(" -> ");
  ret.append(tradedTo.str());
  return ret;
}
}  // namespace cct