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
  friend class ClosestRecentDepositPicker;

  MonetaryAmount _amount;
  TimePoint _timePoint;
  int _originalPos = -1;
};

class ClosestRecentDepositPicker {
 private:
  using RecentDepositVector = SmallVector<RecentDeposit, 4>;

 public:
  using value_type = RecentDepositVector::value_type;
  using size_type = RecentDepositVector::size_type;

  ClosestRecentDepositPicker() noexcept = default;

  void push_back(const RecentDeposit &recentDeposit);

  void push_back(RecentDeposit &&recentDeposit);

  void reserve(size_type sz) { _recentDeposits.reserve(sz); }

  /// Given deposit information in parameters, return a position (0 indexed) corresponding to the closest
  /// deposit that was pushed back. Otherwise, if no matching deposit is found (added previously thanks to 'push_back'
  /// method), -1 is returned
  int pickClosestRecentDepositPos(const RecentDeposit &expectedDeposit);

 private:
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
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::api::RecentDeposit &v, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{} at {}", v.amount(), cct::TimeToString(v.timePoint()));
  }
};
#endif