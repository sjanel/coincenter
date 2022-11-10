#pragma once

#include <cstdint>

#include "cct_flatset.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "market.hpp"
#include "orderid.hpp"
#include "timedef.hpp"
#include "timestring.hpp"

namespace cct {

class OrderConstraintsBitmap {
 public:
  enum class ConstraintType : uint8_t { kCur1, kCur2, kPlacedBefore, kPlacedAfter, kId };

 private:
  static constexpr uint8_t kMarketConstrained =
      (1U << static_cast<uint8_t>(ConstraintType::kCur1)) | (1U << static_cast<uint8_t>(ConstraintType::kCur2));

  static constexpr uint8_t kIdConstrained = (1U << static_cast<uint8_t>(ConstraintType::kId));

 public:
  void set(ConstraintType constraintType) { _bmp |= (1U << static_cast<uint8_t>(constraintType)); }

  bool isConstrained(ConstraintType constraintType) const {
    return _bmp & (1U << static_cast<uint8_t>(constraintType));
  }

  bool empty() const { return _bmp == 0U; }

  bool isMarketDependent() const {
    return isConstrained(ConstraintType::kCur1) && isConstrained(ConstraintType::kCur2);
  }
  bool isMarketOnlyDependent() const { return _bmp == kMarketConstrained; }
  bool isAtMostMarketOnlyDependent() const { return ((_bmp | kMarketConstrained) & ~kMarketConstrained) == 0U; }

  bool isOrderIdOnlyDependent() const { return _bmp == kIdConstrained; }

  bool operator==(const OrderConstraintsBitmap &) const = default;

 private:
  uint8_t _bmp{};
};

class OrdersConstraints {
 public:
  using OrderIdSet = FlatSet<OrderId, std::less<>>;

  /// Build OrdersConstraints based on given filtering information
  explicit OrdersConstraints(CurrencyCode cur1 = CurrencyCode(), CurrencyCode cur2 = CurrencyCode(),
                             Duration minAge = Duration(), Duration maxAge = Duration(),
                             OrderIdSet &&ordersIdSet = OrderIdSet());

  TimePoint placedBefore() const { return _placedBefore; }
  TimePoint placedAfter() const { return _placedAfter; }

  bool isPlacedTimeAfterDefined() const { return _placedAfter != TimePoint::min(); }
  bool isPlacedTimeBeforeDefined() const { return _placedBefore != TimePoint::max(); }

  bool validatePlacedTime(TimePoint t) const { return t >= _placedAfter && t <= _placedBefore; }

  bool validateCur(CurrencyCode cur1, CurrencyCode cur2) const {
    if (_cur1.isNeutral()) {
      return _cur2.isNeutral() || _cur2 == cur1 || _cur2 == cur2;
    }
    if (_cur2.isNeutral()) {
      return _cur1 == cur1 || _cur1 == cur2;
    }
    return (_cur1 == cur1 && _cur2 == cur2) || (_cur1 == cur2 && _cur2 == cur1);
  }

  bool isCur1Defined() const { return !_cur1.isNeutral(); }
  bool isCur2Defined() const { return !_cur2.isNeutral(); }
  bool isMarketDefined() const { return isCur1Defined() && isCur2Defined(); }

  Market market() const { return Market(_cur1, _cur2); }

  string curStr1() const { return _cur1.str(); }
  string curStr2() const { return _cur2.str(); }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  bool validateOrderId(std::string_view orderId) const { return !isOrderIdDefined() || _ordersIdSet.contains(orderId); }

  const OrderIdSet &orderIdSet() const { return _ordersIdSet; }

  bool isOrderIdDefined() const { return !_ordersIdSet.empty(); }

  bool noConstraints() const { return _orderConstraintsBitmap.empty(); }

  bool isOrderIdDependent() const {
    return _orderConstraintsBitmap.isConstrained(OrderConstraintsBitmap::ConstraintType::kId);
  }
  bool isOrderIdOnlyDependent() const { return _orderConstraintsBitmap.isOrderIdOnlyDependent(); }

  bool isMarketDependent() const { return _orderConstraintsBitmap.isMarketDependent(); }
  bool isMarketOnlyDependent() const { return _orderConstraintsBitmap.isMarketOnlyDependent(); }
  bool isAtMostMarketDependent() const { return _orderConstraintsBitmap.isAtMostMarketOnlyDependent(); }

  bool operator==(const OrdersConstraints &) const = default;

  using trivially_relocatable = is_trivially_relocatable<OrderIdSet>::type;

 private:
  OrderIdSet _ordersIdSet;
  TimePoint _placedBefore;
  TimePoint _placedAfter;
  CurrencyCode _cur1;
  CurrencyCode _cur2;
  OrderConstraintsBitmap _orderConstraintsBitmap;
};
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::OrdersConstraints> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::OrdersConstraints &ordersConstraints, FormatContext &ctx) const -> decltype(ctx.out()) {
    if (ordersConstraints.isCur1Defined()) {
      ctx.out() = fmt::format_to(ctx.out(), "{}", ordersConstraints.cur1());
    } else {
      ctx.out() = fmt::format_to(ctx.out(), "any");
    }
    if (ordersConstraints.isCur2Defined()) {
      ctx.out() = fmt::format_to(ctx.out(), "-{}", ordersConstraints.cur2());
    }

    ctx.out() = fmt::format_to(ctx.out(), " currencies");
    if (ordersConstraints.placedBefore() != cct::TimePoint::max()) {
      ctx.out() = fmt::format_to(ctx.out(), " before {}", cct::ToString(ordersConstraints.placedBefore()));
    }
    if (ordersConstraints.placedAfter() != cct::TimePoint::min()) {
      ctx.out() = fmt::format_to(ctx.out(), " after {}", cct::ToString(ordersConstraints.placedAfter()));
    }
    if (ordersConstraints.isOrderIdDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), " matching Ids [{}]", fmt::join(ordersConstraints.orderIdSet(), ", "));
    }
    return ctx.out();
  }
};
#endif