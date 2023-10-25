#pragma once

#include <compare>
#include <string_view>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class Order {
 public:
  Order(const char *id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
        TimePoint placedTime, TradeSide side)
      : Order(OrderId(id), matchedVolume, remainingVolume, price, placedTime, side) {}

  Order(std::string_view id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
        TimePoint placedTime, TradeSide side);

  Order(OrderId &&id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
        TimePoint placedTime, TradeSide side);

  MonetaryAmount originalVolume() const { return _matchedVolume + _remainingVolume; }
  MonetaryAmount matchedVolume() const { return _matchedVolume; }
  MonetaryAmount remainingVolume() const { return _remainingVolume; }
  MonetaryAmount price() const { return _price; }

  OrderId &id() { return _id; }
  const OrderId &id() const { return _id; }

  TimePoint placedTime() const { return _placedTime; }

  TradeSide side() const { return _side; }

  std::string_view sideStr() const;

  string placedTimeStr() const;

  Market market() const { return Market(_matchedVolume.currencyCode(), _price.currencyCode()); }

  /// default ordering by place time first, then matched volume, etc
  auto operator<=>(const Order &) const = default;

  using trivially_relocatable = is_trivially_relocatable<OrderId>::type;

 private:
  TimePoint _placedTime;
  OrderId _id;  // exchange internal id, format specific to each exchange
  MonetaryAmount _matchedVolume;
  MonetaryAmount _remainingVolume;
  MonetaryAmount _price;
  TradeSide _side;
};
}  // namespace cct