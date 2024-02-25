#pragma once

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

class ClosedOrder : public Order {
 public:
  ClosedOrder(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime,
              TimePoint matchedTime, TradeSide side);

  TimePoint matchedTime() const { return _matchedTime; }

  string matchedTimeStr() const;

 private:
  TimePoint _matchedTime;
};
}  // namespace cct