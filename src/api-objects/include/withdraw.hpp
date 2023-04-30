#pragma once

#include "withdrawordeposit.hpp"

namespace cct {
class Withdraw : public WithdrawOrDeposit {
 public:
  template <class StringType>
  Withdraw(StringType &&id, TimePoint time, MonetaryAmount netEmittedAmount, Status status, MonetaryAmount withdrawFee)
      : WithdrawOrDeposit(std::forward<StringType>(id), time, netEmittedAmount, status), _withdrawFee(withdrawFee) {}

  MonetaryAmount withdrawFee() const { return _withdrawFee; }

 private:
  MonetaryAmount _withdrawFee;
};
}  // namespace cct