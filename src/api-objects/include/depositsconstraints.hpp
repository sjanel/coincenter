#pragma once

#include "cct_flatset.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {
class DepositsConstraintsBitmap {
 public:
  enum class ConstraintType : uint8_t { kCur, kReceivedBefore, kReceivedAfter, kId };

 private:
  static constexpr uint8_t kIdConstrained = 1U << static_cast<uint8_t>(ConstraintType::kId);
  static constexpr uint8_t kCurConstrained = 1U << static_cast<uint8_t>(ConstraintType::kCur);

 public:
  void set(ConstraintType constraintType) { _bmp |= (1U << static_cast<uint8_t>(constraintType)); }

  bool isConstrained(ConstraintType constraintType) const {
    return _bmp & (1U << static_cast<uint8_t>(constraintType));
  }

  bool empty() const { return _bmp == 0U; }

  bool isCurDependent() const { return isConstrained(ConstraintType::kCur); }
  bool isCurOnlyDependent() const { return _bmp == kCurConstrained; }
  bool isAtMostCurOnlyDependent() const { return ((_bmp | kCurConstrained) & ~kCurConstrained) == 0U; }

  bool isDepositIdOnlyDependent() const { return _bmp == kIdConstrained; }

  bool operator==(const DepositsConstraintsBitmap &) const = default;

 private:
  uint8_t _bmp{};
};

class DepositsConstraints {
 public:
  using DepositIdSet = FlatSet<string, std::less<>>;

  explicit DepositsConstraints(CurrencyCode currencyCode = CurrencyCode(), Duration minAge = Duration(),
                               Duration maxAge = Duration(), DepositIdSet &&depositIdSet = DepositIdSet());

  TimePoint receivedBefore() const { return _receivedBefore; }
  TimePoint receivedAfter() const { return _receivedAfter; }

  bool isReceivedTimeAfterDefined() const { return _receivedAfter != TimePoint::min(); }
  bool isReceivedTimeBeforeDefined() const { return _receivedBefore != TimePoint::max(); }

  bool noConstraints() const { return _depositsConstraintsBmp.empty(); }

  CurrencyCode currencyCode() const { return _currencyCode; }

  bool validateCur(CurrencyCode cur) const { return _currencyCode.isNeutral() || cur == _currencyCode; }
  bool validateReceivedTime(TimePoint t) const { return t >= _receivedAfter && t <= _receivedBefore; }

  const DepositIdSet &depositIdSet() const { return _depositIdSet; }

  bool isCurDefined() const { return !_currencyCode.isNeutral(); }
  bool isDepositIdDefined() const { return !_depositIdSet.empty(); }
  bool isAtMostCurDependent() const { return _depositsConstraintsBmp.isAtMostCurOnlyDependent(); }

  bool isOrderIdDependent() const {
    return _depositsConstraintsBmp.isConstrained(DepositsConstraintsBitmap::ConstraintType::kId);
  }
  bool isDepositIdOnlyDependent() const { return _depositsConstraintsBmp.isDepositIdOnlyDependent(); }

  using trivially_relocatable = is_trivially_relocatable<DepositIdSet>::type;

 private:
  DepositIdSet _depositIdSet;
  TimePoint _receivedBefore;
  TimePoint _receivedAfter;
  CurrencyCode _currencyCode;
  DepositsConstraintsBitmap _depositsConstraintsBmp;
};
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::DepositsConstraints> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::DepositsConstraints &depositsConstraints, FormatContext &ctx) const -> decltype(ctx.out()) {
    if (depositsConstraints.isCurDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), "{} currency", depositsConstraints.currencyCode());
    } else {
      ctx.out() = fmt::format_to(ctx.out(), "any currency");
    }

    if (depositsConstraints.receivedBefore() != cct::TimePoint::max()) {
      ctx.out() = fmt::format_to(ctx.out(), " before {}", cct::ToString(depositsConstraints.receivedBefore()));
    }
    if (depositsConstraints.receivedAfter() != cct::TimePoint::min()) {
      ctx.out() = fmt::format_to(ctx.out(), " after {}", cct::ToString(depositsConstraints.receivedAfter()));
    }
    if (depositsConstraints.isDepositIdDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), " matching Ids [{}]", fmt::join(depositsConstraints.depositIdSet(), ", "));
    }
    return ctx.out();
  }
};
#endif