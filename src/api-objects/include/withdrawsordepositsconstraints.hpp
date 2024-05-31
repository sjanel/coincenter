#pragma once

#include "baseconstraints.hpp"
#include "cct_flatset.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {

class WithdrawsOrDepositsConstraints {
 public:
  using IdSet = FlatSet<string, std::less<>>;

  explicit WithdrawsOrDepositsConstraints(CurrencyCode currencyCode = CurrencyCode(),
                                          Duration minAge = kUndefinedDuration, Duration maxAge = kUndefinedDuration,
                                          IdSet &&idSet = IdSet());

  // Creates a WithdrawsOrDepositsConstraints based on a single transaction id and currency code.
  // Useful for retrieval of a specific Deposit / Withdraw.
  WithdrawsOrDepositsConstraints(CurrencyCode currencyCode, std::string_view id);

  TimePoint timeBefore() const { return _timeBefore; }
  TimePoint timeAfter() const { return _timeAfter; }

  bool isTimeAfterDefined() const { return _timeAfter != TimePoint::min(); }
  bool isTimeBeforeDefined() const { return _timeBefore != TimePoint::max(); }

  bool noConstraints() const { return _currencyIdTimeConstraintsBmp.empty(); }

  CurrencyCode currencyCode() const { return _currencyCode; }

  bool validateCur(CurrencyCode cur) const { return currencyCode().isNeutral() || cur == currencyCode(); }

  bool validateTime(TimePoint tp) const { return tp >= _timeAfter && tp <= _timeBefore; }

  bool validateId(std::string_view id) const { return !isIdDefined() || _idSet.contains(id); }

  const IdSet &idSet() const { return _idSet; }

  bool isCurDefined() const { return !_currencyCode.isNeutral(); }
  bool isIdDefined() const { return !_idSet.empty(); }
  bool isAtMostCurDependent() const { return _currencyIdTimeConstraintsBmp.isAtMostCurOnlyDependent(); }

  bool isIdDependent() const {
    return _currencyIdTimeConstraintsBmp.isConstrained(CurrencyIdTimeConstraintsBmp::ConstraintType::kId);
  }
  bool isIdOnlyDependent() const { return _currencyIdTimeConstraintsBmp.isDepositIdOnlyDependent(); }

  bool operator==(const WithdrawsOrDepositsConstraints &) const noexcept = default;

  using trivially_relocatable = is_trivially_relocatable<IdSet>::type;

 private:
  IdSet _idSet;
  TimePoint _timeBefore{TimePoint::max()};
  TimePoint _timeAfter{TimePoint::min()};
  CurrencyCode _currencyCode;
  CurrencyIdTimeConstraintsBmp _currencyIdTimeConstraintsBmp;
};
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::WithdrawsOrDepositsConstraints> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::WithdrawsOrDepositsConstraints &constraints, FormatContext &ctx) const -> decltype(ctx.out()) {
    if (constraints.isCurDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), "{} currency", constraints.currencyCode());
    } else {
      ctx.out() = fmt::format_to(ctx.out(), "any currency");
    }

    if (constraints.timeBefore() != cct::TimePoint::max()) {
      ctx.out() = fmt::format_to(ctx.out(), " before {}", cct::TimeToString(constraints.timeBefore()));
    }
    if (constraints.timeAfter() != cct::TimePoint::min()) {
      ctx.out() = fmt::format_to(ctx.out(), " after {}", cct::TimeToString(constraints.timeAfter()));
    }
    if (constraints.isIdDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), " matching Ids [{}]", fmt::join(constraints.idSet(), ", "));
    }
    return ctx.out();
  }
};
#endif