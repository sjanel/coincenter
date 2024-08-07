#include "withdrawoptions.hpp"

#include <string_view>

#include "timedef.hpp"
#include "unreachable.hpp"

namespace cct {
WithdrawOptions::WithdrawOptions(Duration withdrawRefreshTime, WithdrawSyncPolicy withdrawSyncPolicy, Mode mode)
    : _withdrawRefreshTime(withdrawRefreshTime), _withdrawSyncPolicy(withdrawSyncPolicy), _mode(mode) {}

std::string_view WithdrawOptions::withdrawSyncPolicyStr() const {
  switch (_withdrawSyncPolicy) {
    case WithdrawSyncPolicy::kSynchronous:
      return "synchronous";
    case WithdrawSyncPolicy::kAsynchronous:
      return "asynchronous";
    default:
      unreachable();
  }
}
}  // namespace cct