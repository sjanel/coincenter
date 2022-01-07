#include "order.hpp"

#include "timestring.hpp"
#include "unreachable.hpp"

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

std::string_view Order::sideStr() const {
  switch (_side) {
    case TradeSide::kBuy:
      return "Buy";
    case TradeSide::kSell:
      return "Sell";
    default:
      unreachable();
  }
}

string Order::placedTimeStr() const { return ToString(_placedTime); }
}  // namespace cct