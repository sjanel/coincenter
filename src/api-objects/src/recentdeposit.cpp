#include "recentdeposit.hpp"

#include <algorithm>

#include "cct_log.hpp"
#include "timestring.hpp"

namespace cct {
const RecentDeposit *RecentDeposit::selectClosestRecentDeposit(RecentDepositVector &recentDeposits) const {
  if (recentDeposits.empty()) {
    log::debug("No recent deposits yet");
    return nullptr;
  }

  // First step: sort from most recent to oldest
  std::ranges::sort(recentDeposits, [](const auto &lhs, const auto &rhs) { return lhs.timePoint() > rhs.timePoint(); });

  // Heuristic - before considering the amounts, only take the most recent deposits (1 day as upper security bound to
  // avoid potential UTC differences)
  auto endIt = std::ranges::partition_point(recentDeposits, [this](const RecentDeposit &deposit) {
    return deposit.timePoint() + std::chrono::days(1) > this->timePoint();
  });

  if (endIt == recentDeposits.begin()) {
    log::debug("Found no time eligible recent deposit");
    return nullptr;
  }

  if (recentDeposits.front().amount() == amount()) {
    log::debug("Found recent deposit {} with exact amount", recentDeposits.front().str());
    return std::addressof(recentDeposits.front());
  }

  // Sort by amount difference
  std::sort(recentDeposits.begin(), endIt, [this](const auto &lhs, const auto &rhs) {
    auto diffLhs = (lhs.amount() - this->amount()).abs();
    auto diffRhs = (rhs.amount() - this->amount()).abs();
    if (diffLhs != diffRhs) {
      return diffLhs < diffRhs;
    }
    // if same amount, prefer the most recent deposit
    return lhs.timePoint() > rhs.timePoint();
  });

  static constexpr double kMaxRelativeDifferenceForSelection = 0.001;

  double closestAmount = recentDeposits.front().amount().toDouble();
  double ourAmount = amount().toDouble();
  double boundMin = ourAmount * (1.0 - kMaxRelativeDifferenceForSelection);
  double boundMax = ourAmount * (1.0 + kMaxRelativeDifferenceForSelection);

  assert(boundMin >= 0 && boundMax >= 0);
  if (closestAmount > boundMin && closestAmount < boundMax) {
    log::debug("Found recent deposit {} with close amount", recentDeposits.front().str());
    return std::addressof(recentDeposits.front());
  }
  log::debug("Found no recent deposit with close amount");
  return nullptr;
}

string RecentDeposit::str() const {
  string ret(_amount.str());
  ret.append(" at ");
  ret.append(ToString(_timePoint));
  return ret;
}
}  // namespace cct