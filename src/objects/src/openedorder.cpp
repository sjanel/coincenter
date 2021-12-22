#include "openedorder.hpp"

#include "timestring.hpp"
#include "unreachable.hpp"

namespace cct {
std::string_view OpenedOrder::sideStr() const {
  switch (_side) {
    case TradeSide::kBuy:
      return "Buy";
    case TradeSide::kSell:
      return "Sell";
    default:
      unreachable();
  }
}

string OpenedOrder::placedTimeStr() const { return ToString(_placedTime); }
}  // namespace cct