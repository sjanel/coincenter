#pragma once

#include <compare>
#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {
class WithdrawOrDeposit {
 public:
  enum class Status : int8_t {
    kSuccess,
    kProcessing,
    kFailureOrRejected,
  };

  template <class StringType>
  WithdrawOrDeposit(StringType &&id, TimePoint time, MonetaryAmount amount, Status status)
      : _time(time), _id(std::forward<StringType>(id)), _amount(amount), _status(status) {}

  TimePoint time() const { return _time; }

  std::string_view id() const { return _id; }

  MonetaryAmount amount() const { return _amount; }

  Status status() const { return _status; }

  std::string_view statusStr() const;

  string timeStr() const;

  /// default ordering by received time first
  auto operator<=>(const WithdrawOrDeposit &) const = default;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  TimePoint _time;
  string _id;
  MonetaryAmount _amount;
  Status _status;
};

}  // namespace cct
