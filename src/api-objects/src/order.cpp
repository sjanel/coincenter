#include "order.hpp"

#include <string_view>
#include <utility>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct {

Order::Order(std::string_view id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
             TimePoint placedTime, TradeSide side)
    : _placedTime(placedTime),
      _id(id),
      _matchedVolume(matchedVolume),
      _remainingVolume(remainingVolume),
      _price(price),
      _side(side) {}

Order::Order(string &&id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
             TimePoint placedTime, TradeSide side)
    : _placedTime(placedTime),
      _id(std::move(id)),
      _matchedVolume(matchedVolume),
      _remainingVolume(remainingVolume),
      _price(price),
      _side(side) {}

std::string_view Order::sideStr() const { return SideStr(_side); }

string Order::placedTimeStr() const { return ToString(_placedTime); }
}  // namespace cct