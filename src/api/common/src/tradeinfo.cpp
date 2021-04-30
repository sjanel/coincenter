#include "tradeinfo.hpp"

namespace cct {
namespace api {
TradedOrdersInfo TradedOrdersInfo::operator+(const TradedOrdersInfo &o) const {
  return TradedOrdersInfo(tradedFrom + o.tradedFrom, tradedTo + o.tradedTo);
}
}  // namespace api
}  // namespace cct