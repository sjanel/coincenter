#pragma once

#include <string_view>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "wallet.hpp"

namespace cct {
namespace api {
class InitiatedWithdrawInfo {
 public:
  /// Empty InitiatedWithdrawInfo, when no withdrawal has been done
  InitiatedWithdrawInfo(string &&msg = string()) : _withdrawIdOrMsgIfNotInitiated(std::move(msg)) {}

  InitiatedWithdrawInfo(Wallet receivingWallet, string withdrawId, MonetaryAmount grossEmittedAmount,
                        TimePoint initiatedTime = Clock::now());

  TimePoint initiatedTime() const { return _initiatedTime; }

  const Wallet &receivingWallet() const { return _receivingWallet; }

  std::string_view withdrawId() const { return _withdrawIdOrMsgIfNotInitiated; }

  MonetaryAmount grossEmittedAmount() const { return _grossEmittedAmount; }

  using trivially_relocatable =
      std::integral_constant<bool, is_trivially_relocatable_v<Wallet> && is_trivially_relocatable_v<string>>::type;

 private:
  Wallet _receivingWallet;
  string _withdrawIdOrMsgIfNotInitiated;
  MonetaryAmount _grossEmittedAmount;
  TimePoint _initiatedTime{};  // The time at which withdraw has been ordered from the source exchange
};

class SentWithdrawInfo {
 public:
  SentWithdrawInfo() noexcept(std::is_nothrow_default_constructible_v<MonetaryAmount>) = default;

  SentWithdrawInfo(MonetaryAmount netEmittedAmount, MonetaryAmount fee, bool isWithdrawSent)
      : _netEmittedAmount(netEmittedAmount), _fee(fee), _isWithdrawSent(isWithdrawSent) {}

  MonetaryAmount netEmittedAmount() const { return _netEmittedAmount; }

  MonetaryAmount fee() const { return _fee; }

  bool isWithdrawSent() const { return _isWithdrawSent; }

 private:
  MonetaryAmount _netEmittedAmount;
  MonetaryAmount _fee;
  bool _isWithdrawSent = false;
};

class ReceivedWithdrawInfo {
 public:
  ReceivedWithdrawInfo() noexcept(std::is_nothrow_default_constructible_v<MonetaryAmount>) = default;

  ReceivedWithdrawInfo(MonetaryAmount netReceivedAmount, bool isWithdrawReceived)
      : _netReceivedAmount(netReceivedAmount), _isWithdrawReceived(isWithdrawReceived) {}

  MonetaryAmount netReceivedAmount() const { return _netReceivedAmount; }

  bool isWithdrawReceived() const { return _isWithdrawReceived; }

 private:
  MonetaryAmount _netReceivedAmount;
  bool _isWithdrawReceived = false;
};

}  // namespace api

class WithdrawInfo {
 public:
  /// Empty withdraw info, when no withdrawal has been done
  explicit WithdrawInfo(string &&msg = string()) : _initiatedWithdrawInfo(std::move(msg)) {}

  /// Constructs a withdraw info with all information
  WithdrawInfo(api::InitiatedWithdrawInfo &&initiatedWithdrawInfo, MonetaryAmount receivedAmount,
               TimePoint receivedTime = Clock::now());

  TimePoint initiatedTime() const { return _initiatedWithdrawInfo.initiatedTime(); }

  bool hasBeenInitiated() const { return initiatedTime() != TimePoint{}; }

  TimePoint receivedTime() const { return _receivedTime; }

  const Wallet &receivingWallet() const { return _initiatedWithdrawInfo.receivingWallet(); }

  MonetaryAmount grossAmount() const { return _initiatedWithdrawInfo.grossEmittedAmount(); }

  MonetaryAmount receivedAmount() const { return _receivedAmount; }

  std::string_view withdrawId() const;

  using trivially_relocatable =
      std::integral_constant<bool, is_trivially_relocatable_v<Wallet> && is_trivially_relocatable_v<string>>::type;

 private:
  api::InitiatedWithdrawInfo _initiatedWithdrawInfo;
  TimePoint _receivedTime{};       // time at which destination provides received funds as available for trade
  MonetaryAmount _receivedAmount;  // fee deduced amount that destination will receive
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::WithdrawInfo> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::WithdrawInfo &wi, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "[{}] -> [{}]@{:ek}", wi.grossAmount(), wi.receivedAmount(),
                          wi.receivingWallet().exchangeName());
  }
};
#endif