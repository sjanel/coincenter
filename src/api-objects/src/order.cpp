#include "order.hpp"

#include <string_view>
#include <utility>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct {

Order::Order(string id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime, TradeSide side)
    : _placedTime(placedTime), _id(std::move(id)), _matchedVolume(matchedVolume), _price(price), _side(side) {}

std::string_view Order::sideStr() const { return SideStr(_side); }

string Order::placedTimeStr() const { return TimeToString(_placedTime); }
}  // namespace cct