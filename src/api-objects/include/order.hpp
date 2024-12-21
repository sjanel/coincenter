#pragma once

#include <compare>

#include "cct_type_traits.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class Order {
 public:
  TimePoint placedTime() const { return _placedTime; }

  OrderId &id() { return _id; }
  const OrderId &id() const { return _id; }

  MonetaryAmount matchedVolume() const { return _matchedVolume; }
  MonetaryAmount price() const { return _price; }

  TradeSide side() const { return _side; }

  Market market() const { return Market(_matchedVolume.currencyCode(), _price.currencyCode()); }

  std::strong_ordering operator<=>(const Order &) const noexcept = default;

  using trivially_relocatable = is_trivially_relocatable<OrderId>::type;

 protected:
  Order(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime, TradeSide side)
      : _placedTime(placedTime), _id(std::move(id)), _matchedVolume(matchedVolume), _price(price), _side(side) {}

 private:
  TimePoint _placedTime;
  OrderId _id;  // exchange internal id, format specific to each exchange
  MonetaryAmount _matchedVolume;
  MonetaryAmount _price;
  TradeSide _side;
};
}  // namespace cct