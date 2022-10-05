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
  InitiatedWithdrawInfo(Wallet receivingWallet, WithdrawIdView withdrawId, MonetaryAmount grossEmittedAmount,
                        TimePoint initiatedTime = Clock::now());

  TimePoint initiatedTime() const { return _initiatedTime; }

  const Wallet &receivingWallet() const { return _receivingWallet; }

  const WithdrawId &withdrawId() const { return _withdrawIdOrMsgIfNotInitiated; }

  MonetaryAmount grossEmittedAmount() const { return _grossEmittedAmount; }

 private:
  Wallet _receivingWallet;
  WithdrawId _withdrawIdOrMsgIfNotInitiated;
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
  /// Empty withdraw info, when no withdrawal has been done
  explicit WithdrawInfo(string &&msg = string()) : _withdrawIdOrMsgIfNotInitiated(std::move(msg)) {}

  /// Constructs a withdraw info with all information
  WithdrawInfo(const api::InitiatedWithdrawInfo &initiatedWithdrawInfo, const api::SentWithdrawInfo &sentWithdrawInfo,
               TimePoint receivedTime = Clock::now());

  bool hasBeenInitiated() const { return _initiatedTime != TimePoint{}; }

  TimePoint initiatedTime() const { return _initiatedTime; }

  TimePoint receivedTime() const { return _receivedTime; }

  const Wallet &receivingWallet() const { return _receivingWallet; }

  MonetaryAmount grossAmount() const { return _grossAmount; }

  MonetaryAmount netEmittedAmount() const { return _netEmittedAmount; }

  const WithdrawId &withdrawId() const;

  std::string_view withdrawStatus() const {
    return hasBeenInitiated() ? std::string_view("OK") : std::string_view(_withdrawIdOrMsgIfNotInitiated);
  }

 private:
  Wallet _receivingWallet;
  WithdrawId _withdrawIdOrMsgIfNotInitiated;
  TimePoint _initiatedTime{};        // The time at which withdraw has been ordered from the source exchange
  TimePoint _receivedTime{};         // time at which destination provides received funds as available for trade
  MonetaryAmount _grossAmount;       // Gross amount including fees which will be considered for the withdraw
  MonetaryAmount _netEmittedAmount;  // fee deduced amount that destination will receive
};
}  // namespace cct