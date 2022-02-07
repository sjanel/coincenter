#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "wallet.hpp"

namespace cct {
using WithdrawId = string;
using WithdrawIdView = std::string_view;

namespace api {
class InitiatedWithdrawInfo {
 public:
  InitiatedWithdrawInfo(const Wallet &receivingWallet, WithdrawIdView withdrawId, MonetaryAmount grossEmittedAmount)
      : _receivingWallet(receivingWallet),
        _withdrawId(withdrawId),
        _initiatedTime(Clock::now()),
        _grossEmittedAmount(grossEmittedAmount) {}

  InitiatedWithdrawInfo(Wallet &&receivingWallet, WithdrawIdView withdrawId, MonetaryAmount grossEmittedAmount)
      : _receivingWallet(std::move(receivingWallet)),
        _withdrawId(withdrawId),
        _initiatedTime(Clock::now()),
        _grossEmittedAmount(grossEmittedAmount) {}

  TimePoint initiatedTime() const { return _initiatedTime; }

  const Wallet &receivingWallet() const { return _receivingWallet; }

  const WithdrawId &withdrawId() const { return _withdrawId; }

  MonetaryAmount grossEmittedAmount() const { return _grossEmittedAmount; }

 private:
  Wallet _receivingWallet;
  WithdrawId _withdrawId;
  TimePoint _initiatedTime;  // The time at which withdraw has been ordered from the source exchange
  MonetaryAmount _grossEmittedAmount;
};

class SentWithdrawInfo {
 public:
  SentWithdrawInfo() noexcept(std::is_nothrow_default_constructible_v<MonetaryAmount>) = default;

  SentWithdrawInfo(MonetaryAmount netEmittedAmount, bool isWithdrawSent)
      : _netEmittedAmount(netEmittedAmount), _isWithdrawSent(isWithdrawSent) {}

  bool isWithdrawSent() const { return _isWithdrawSent; }

  MonetaryAmount netEmittedAmount() const { return _netEmittedAmount; }

 private:
  MonetaryAmount _netEmittedAmount;
  bool _isWithdrawSent = false;
};

}  // namespace api

class WithdrawInfo {
 public:
  WithdrawInfo(const api::InitiatedWithdrawInfo &initiatedWithdrawInfo, const api::SentWithdrawInfo &sentWithdrawInfo)
      : _receivingWallet(initiatedWithdrawInfo.receivingWallet()),
        _withdrawId(initiatedWithdrawInfo.withdrawId()),
        _initiatedTime(initiatedWithdrawInfo.initiatedTime()),
        _receivedTime(Clock::now()),
        _netEmittedAmount(sentWithdrawInfo.netEmittedAmount()) {}

  TimePoint initiatedTime() const { return _initiatedTime; }

  TimePoint receivedTime() const { return _receivedTime; }

  const Wallet &receivingWallet() const { return _receivingWallet; }

  MonetaryAmount netEmittedAmount() const { return _netEmittedAmount; }

  const WithdrawId withdrawId() const { return _withdrawId; }

 private:
  Wallet _receivingWallet;
  WithdrawId _withdrawId;
  TimePoint _initiatedTime;          // The time at which withdraw has been ordered from the source exchange
  TimePoint _receivedTime;           // time at which destination provides received funds as available for trade
  MonetaryAmount _netEmittedAmount;  // fee deduced amount that destination will receive
};
}  // namespace cct