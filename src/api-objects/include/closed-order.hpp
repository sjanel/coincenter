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

  /// Compute the resulting merged closed order from *this and given one.
  /// Given closed order should be of same ID, TradeSide and Market.
  [[nodiscard]] ClosedOrder mergeWith(const ClosedOrder &closedOrder) const;

 private:
  TimePoint _matchedTime;
};
}  // namespace cct