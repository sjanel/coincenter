#include "closed-order.hpp"

#include <utility>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "order.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct {

ClosedOrder::ClosedOrder(OrderId id, MonetaryAmount matchedVolume, MonetaryAmount price, TimePoint placedTime,
                         TimePoint matchedTime, TradeSide side)
    : Order(std::move(id), matchedVolume, price, placedTime, side), _matchedTime(matchedTime) {}

string ClosedOrder::matchedTimeStr() const { return ToString(_matchedTime); }

ClosedOrder ClosedOrder::mergeWith(const ClosedOrder &closedOrder) const {
  const MonetaryAmount totalMatchedVolume = closedOrder.matchedVolume() + matchedVolume();
  const auto previousMatchedTs = TimestampToMs(matchedTime());
  const auto currentMatchedTs = TimestampToMs(closedOrder.matchedTime());
  const auto avgMatchedTs = (((previousMatchedTs * matchedVolume().toNeutral()) +
                              (currentMatchedTs * closedOrder.matchedVolume().toNeutral())) /
                             totalMatchedVolume.toNeutral())
                                .integerPart();
  const TimePoint avgMatchedTime{TimeInMs{avgMatchedTs}};

  MonetaryAmount avgPrice = price();
  if (closedOrder.price() != price()) {
    avgPrice =
        ((matchedVolume().toNeutral() * price()) + (closedOrder.matchedVolume().toNeutral() * closedOrder.price())) /
        totalMatchedVolume.toNeutral();
  }
  return ClosedOrder(id(), totalMatchedVolume, avgPrice, placedTime(), avgMatchedTime, side());
}
}  // namespace cct