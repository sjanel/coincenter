#pragma once

#include "cct_format.hpp"
#include "cct_smallvector.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct::api {
class RecentDeposit {
 public:
  RecentDeposit() noexcept = default;

  RecentDeposit(MonetaryAmount amount, TimePoint timePoint) : _amount(amount), _timePoint(timePoint) {}

  MonetaryAmount amount() const { return _amount; }
  TimePoint timePoint() const { return _timePoint; }

 private:
  MonetaryAmount _amount;
  TimePoint _timePoint{};
};

class ClosestRecentDepositPicker {
 public:
  ClosestRecentDepositPicker() noexcept = default;

  void addDeposit(const RecentDeposit &recentDeposit);

  /// Given deposit information in parameters, return a RecentDeposit corresponding to the closest
  /// deposit. Otherwise, if no matching deposit is found (added previously thanks to 'addDeposit' method),
  /// a default RecentDeposit is returned.
  RecentDeposit pickClosestRecentDepositOrDefault(const RecentDeposit &expectedDeposit);

 private:
  using RecentDepositVector = SmallVector<RecentDeposit, 4>;

  /// Select the RecentDeposit among given ones which is the closest to 'this' object.
  /// It may reorder the given vector but will not modify objects themselves.
  /// Returns nullptr if no matching deposit has been found
  const RecentDeposit *selectClosestRecentDeposit(const RecentDeposit &expectedDeposit);

  RecentDepositVector _recentDeposits;
};

}  // namespace cct::api

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::api::RecentDeposit> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::api::RecentDeposit &v, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{} at {}", v.amount(), cct::ToString(v.timePoint()));
  }
};
#endif