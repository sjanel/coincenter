#pragma once

#include <cstdint>
#include <string_view>

#include "timedef.hpp"

namespace cct {

enum class WithdrawSyncPolicy : int8_t {
  kSynchronous,  // Follow lifetime of the withdraw until funds are received at destination
  kAsynchronous  // Only trigger withdraw and exit withdraw process directly
};

class WithdrawOptions {
 public:
  constexpr WithdrawOptions() noexcept = default;

  enum class Mode : int8_t { kSimulation, kReal };

  WithdrawOptions(Duration withdrawRefreshTime, WithdrawSyncPolicy withdrawSyncPolicy, Mode mode);

  constexpr Duration withdrawRefreshTime() const { return _withdrawRefreshTime; }

  constexpr WithdrawSyncPolicy withdrawSyncPolicy() const { return _withdrawSyncPolicy; }

  std::string_view withdrawSyncPolicyStr() const;

  constexpr Mode mode() const { return _mode; }

  bool operator==(const WithdrawOptions &) const noexcept = default;

 private:
  /// The waiting time between each query of withdraw info to check withdraw status from an exchange.
  /// A very small value is not relevant as withdraw time order of magnitude are minutes or hours
  static constexpr auto kWithdrawRefreshTime = seconds(5);

  Duration _withdrawRefreshTime = kWithdrawRefreshTime;
  WithdrawSyncPolicy _withdrawSyncPolicy = WithdrawSyncPolicy::kSynchronous;
  Mode _mode = Mode::kReal;
};
}  // namespace cct