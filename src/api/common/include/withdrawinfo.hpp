#pragma once

#include <chrono>

#include "monetaryamount.hpp"

namespace cct {
class WithdrawInfo {
 public:
  using Clock = std::chrono::high_resolution_clock;
  using TimePoint = std::chrono::time_point<Clock>;

  WithdrawInfo(const Wallet &receivingWallet, TimePoint initiatedTime, MonetaryAmount netEmittedAmount)
      : _receivingWallet(receivingWallet), _initiatedTime(initiatedTime), _netEmittedAmount(netEmittedAmount) {}

  WithdrawInfo(Wallet &&receivingWallet, TimePoint initiatedTime, MonetaryAmount netEmittedAmount)
      : _receivingWallet(std::move(receivingWallet)),
        _initiatedTime(initiatedTime),
        _netEmittedAmount(netEmittedAmount) {}

  TimePoint initiatedTime() const { return _initiatedTime; }

 private:
  Wallet _receivingWallet;
  TimePoint _initiatedTime;  // The time at which withdraw has been ordered from the source exchange
  MonetaryAmount _netEmittedAmount;
};
}  // namespace cct