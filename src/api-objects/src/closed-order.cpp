#include "closed-order.hpp"

#include <utility>

#include "monetaryamount.hpp"
#include "order.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

ClosedOrder::ClosedOrder(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime,
                         TimePoint matchedTime, TradeSide side)
    : Order(std::move(id), matchedVolume, price, placedTime, side), _matchedTime(matchedTime) {}

ClosedOrder ClosedOrder::mergeWith(const ClosedOrder &closedOrder) const {
  const MonetaryAmount totalMatchedVolume = closedOrder.matchedVolume() + matchedVolume();
  const auto previousMatchedTs = TimestampToMillisecondsSinceEpoch(matchedTime());
  const auto currentMatchedTs = TimestampToMillisecondsSinceEpoch(closedOrder.matchedTime());
  const auto avgMatchedTs = (((previousMatchedTs * matchedVolume().toNeutral()) +
                              (currentMatchedTs * closedOrder.matchedVolume().toNeutral())) /
                             totalMatchedVolume.toNeutral())
                                .integerPart();
  const TimePoint avgMatchedTime{milliseconds{avgMatchedTs}};

  MonetaryAmount avgPrice = price();
  if (closedOrder.price() != price()) {
    avgPrice =
        ((matchedVolume().toNeutral() * price()) + (closedOrder.matchedVolume().toNeutral() * closedOrder.price())) /
        totalMatchedVolume.toNeutral();
  }
  return {id(), totalMatchedVolume, avgPrice, placedTime(), avgMatchedTime, side()};
}
}  // namespace cct