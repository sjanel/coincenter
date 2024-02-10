#pragma once

#include <compare>

#include "cct_string.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "tradeside.hpp"

namespace cct {

/// Class representing an executed trade that happened on one exchange, not necessarily ours.
class PublicTrade {
 public:
  PublicTrade(TradeSide side, MonetaryAmount amount, MonetaryAmount price, TimePoint time)
      : _time(time), _amount(amount), _price(price), _side(side) {}

  TradeSide side() const { return _side; }

  Market market() const { return {_amount.currencyCode(), _price.currencyCode()}; }

  MonetaryAmount amount() const { return _amount; }

  MonetaryAmount price() const { return _price; }

  TimePoint time() const { return _time; }

  string timeStr() const;

  /// 3 way operator - make compiler generate all 6 operators (including == and !=)
  /// we order by time first, then amount, price, etc. Do not change the fields order!
  std::strong_ordering operator<=>(const PublicTrade &) const = default;

 private:
  TimePoint _time;
  MonetaryAmount _amount;
  MonetaryAmount _price;
  TradeSide _side;
};
}  // namespace cct
