#pragma once

#include <ostream>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

struct TradedAmounts {
  constexpr TradedAmounts() noexcept(std::is_nothrow_default_constructible_v<MonetaryAmount>) = default;

  constexpr TradedAmounts(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode)
      : tradedFrom(0, fromCurrencyCode), tradedTo(0, toCurrencyCode) {}

  constexpr TradedAmounts(MonetaryAmount fromAmount, MonetaryAmount toAmount)
      : tradedFrom(fromAmount), tradedTo(toAmount) {}

  TradedAmounts operator+(const TradedAmounts &o) const {
    return TradedAmounts(tradedFrom + o.tradedFrom, tradedTo + o.tradedTo);
  }
  TradedAmounts &operator+=(const TradedAmounts &o) {
    *this = *this + o;
    return *this;
  }

  constexpr bool operator==(const TradedAmounts &) const = default;

  friend std::ostream &operator<<(std::ostream &os, const TradedAmounts &tradedAmounts);

  string str() const;

  MonetaryAmount tradedFrom;  // In currency of 'from' amount
  MonetaryAmount tradedTo;    // In the opposite currency
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::TradedAmounts> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin(), end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::TradedAmounts &a, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{} -> {}", a.tradedFrom, a.tradedTo);
  }
};
#endif