#pragma once

#include <chrono>
#include <compare>

#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeside.hpp"

namespace cct {
class OpenedOrder {
 public:
  using TimePoint = std::chrono::system_clock::time_point;

  OpenedOrder(MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price, TimePoint placedTime,
              TradeSide side)
      : _placedTime(placedTime),
        _matchedVolume(matchedVolume),
        _remainingVolume(remainingVolume),
        _price(price),
        _side(side) {}

  MonetaryAmount originalVolume() const { return _matchedVolume + _remainingVolume; }
  MonetaryAmount matchedVolume() const { return _matchedVolume; }
  MonetaryAmount remainingVolume() const { return _remainingVolume; }
  MonetaryAmount price() const { return _price; }

  TimePoint placedTime() const { return _placedTime; }

  TradeSide side() const { return _side; }

  std::string_view sideStr() const;

  string placedTimeStr() const;

  Market market() const { return Market(_matchedVolume.currencyCode(), _price.currencyCode()); }

  /// default ordering by place time first, then matched volume, etc
  auto operator<=>(const OpenedOrder &) const = default;

 private:
  TimePoint _placedTime;
  MonetaryAmount _matchedVolume;
  MonetaryAmount _remainingVolume;
  MonetaryAmount _price;
  TradeSide _side;
};
}  // namespace cct