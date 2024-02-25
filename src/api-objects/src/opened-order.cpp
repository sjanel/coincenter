#include "opened-order.hpp"

#include <utility>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

OpenedOrder::OpenedOrder(string id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
                         TimePoint placedTime, TradeSide side)
    : Order(std::move(id), matchedVolume, price, placedTime, side), _remainingVolume(remainingVolume) {}

}  // namespace cct