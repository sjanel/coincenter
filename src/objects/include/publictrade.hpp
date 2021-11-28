#pragma once

#include <chrono>
#include <compare>

#include "cct_string.hpp"
#include "monetaryamount.hpp"

namespace cct {
class PublicTrade {
 public:
  enum class Type : int8_t { kBuy, kSell };

  using TimePoint = std::chrono::system_clock::time_point;

  PublicTrade(Type tradeType, MonetaryAmount amount, MonetaryAmount price, TimePoint time)
      : _time(time), _amount(amount), _price(price), _type(tradeType) {}

  Type type() const { return _type; }

  MonetaryAmount amount() const { return _amount; }
  MonetaryAmount price() const { return _price; }

  TimePoint time() const { return _time; }

  string timeStr() const;

  /// 3 way operator - make compiler generate all 6 operators (including == and !=)
  /// we order by time first, then amount, price, etc. Do not change the fields order!
  auto operator<=>(const PublicTrade &) const = default;

 private:
  TimePoint _time;
  MonetaryAmount _amount;
  MonetaryAmount _price;
  Type _type;
};
}  // namespace cct