#include "tradeinfo.hpp"

namespace cct {
namespace api {
TradedAmounts TradedAmounts::operator+(const TradedAmounts &o) const {
  return TradedAmounts(tradedFrom + o.tradedFrom, tradedTo + o.tradedTo);
}

std::string TradedAmounts::str() const {
  std::string ret;
  ret.append(tradedFrom.str());
  ret.append(" -> ");
  ret.append(tradedTo.str());
  return ret;
}
}  // namespace api
}  // namespace cct