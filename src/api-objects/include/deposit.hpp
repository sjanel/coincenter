#pragma once

#include <compare>
#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class Deposit {
 public:
  template <class StringType>
  Deposit(StringType &&depositId, TimePoint receivedTime, MonetaryAmount amount)
      : _receivedTime(receivedTime), _depositId(std::forward<StringType>(depositId)), _amount(amount) {}

  TimePoint receivedTime() const { return _receivedTime; }

  std::string_view depositId() const { return _depositId; }

  MonetaryAmount amount() const { return _amount; }

  string receivedTimeStr() const;

  /// default ordering by received time first
  auto operator<=>(const Deposit &) const = default;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  TimePoint _receivedTime;
  string _depositId;
  MonetaryAmount _amount;
};
}  // namespace cct