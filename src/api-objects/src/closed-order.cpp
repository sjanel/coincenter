#include "closed-order.hpp"

#include <utility>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct {

ClosedOrder::ClosedOrder(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime,
                         TimePoint matchedTime, TradeSide side)
    : Order(std::move(id), matchedVolume, price, placedTime, side), _matchedTime(matchedTime) {}

string ClosedOrder::matchedTimeStr() const { return ToString(_matchedTime); }
}  // namespace cct