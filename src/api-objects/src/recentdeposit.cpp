#include "recentdeposit.hpp"

#include <algorithm>
#include <chrono>
#include <memory>

#include "cct_log.hpp"

namespace cct::api {

void ClosestRecentDepositPicker::addDeposit(const RecentDeposit &recentDeposit) {
  _recentDeposits.push_back(recentDeposit);
}

RecentDeposit ClosestRecentDepositPicker::pickClosestRecentDepositOrDefault(const RecentDeposit &expectedDeposit) {
  const RecentDeposit *pClosestRecentDeposit = selectClosestRecentDeposit(expectedDeposit);
  if (pClosestRecentDeposit == nullptr) {
    return {};
  }
  return *pClosestRecentDeposit;
}

const RecentDeposit *ClosestRecentDepositPicker::selectClosestRecentDeposit(const RecentDeposit &expectedDeposit) {
  if (_recentDeposits.empty()) {
    log::debug("No recent deposits yet");
    return nullptr;
  }

  // First step: sort from most recent to oldest
  std::ranges::sort(_recentDeposits,
                    [](const auto &lhs, const auto &rhs) { return lhs.timePoint() > rhs.timePoint(); });

  // Heuristic - before considering the amounts, only take the most recent deposits (1 day as upper security bound to
  // avoid potential UTC differences)
  auto endIt = std::ranges::partition_point(_recentDeposits, [&expectedDeposit](const RecentDeposit &deposit) {
    return deposit.timePoint() + std::chrono::days(1) > expectedDeposit.timePoint();
  });

  if (endIt == _recentDeposits.begin()) {
    log::debug("Found no time eligible recent deposit");
    return nullptr;
  }

  if (_recentDeposits.front().amount() == expectedDeposit.amount()) {
    log::debug("Found recent deposit {} with exact amount", _recentDeposits.front());
    return std::addressof(_recentDeposits.front());
  }

  // Sort by amount difference
  std::sort(_recentDeposits.begin(), endIt, [&expectedDeposit](const auto &lhs, const auto &rhs) {
    auto diffLhs = (lhs.amount() - expectedDeposit.amount()).abs();
    auto diffRhs = (rhs.amount() - expectedDeposit.amount()).abs();
    if (diffLhs != diffRhs) {
      return diffLhs < diffRhs;
    }
    // if same amount, prefer the most recent deposit
    return lhs.timePoint() > rhs.timePoint();
  });

  static constexpr double kMaxRelativeDifferenceForSelection = 0.001;

  if (expectedDeposit.amount().isCloseTo(_recentDeposits.front().amount(), kMaxRelativeDifferenceForSelection)) {
    log::debug("Found recent deposit {} with close amount", _recentDeposits.front());
    return std::addressof(_recentDeposits.front());
  }
  log::debug("Found no recent deposit with close amount");
  return nullptr;
}

}  // namespace cct::api