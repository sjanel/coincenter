#pragma once

#include "monetaryamount.hpp"
#include "order.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class OpenedOrder : public Order {
 public:
  OpenedOrder(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount remainingVolume, MonetaryAmount price,
              TimePoint placedTime, TradeSide side);

  MonetaryAmount originalVolume() const { return matchedVolume() + _remainingVolume; }
  MonetaryAmount remainingVolume() const { return _remainingVolume; }

 private:
  MonetaryAmount _remainingVolume;
};
}  // namespace cct