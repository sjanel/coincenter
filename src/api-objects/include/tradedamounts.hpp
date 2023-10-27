#pragma once

#include <ostream>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

struct TradedAmounts {
  constexpr TradedAmounts() noexcept = default;

  constexpr TradedAmounts(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode)
      : from(0, fromCurrencyCode), to(0, toCurrencyCode) {}

  constexpr TradedAmounts(MonetaryAmount fromAmount, MonetaryAmount toAmount) : from(fromAmount), to(toAmount) {}

  TradedAmounts operator+(const TradedAmounts &rhs) const { return {from + rhs.from, to + rhs.to}; }
  TradedAmounts &operator+=(const TradedAmounts &rhs) { return (*this = *this + rhs); }

  constexpr bool operator==(const TradedAmounts &) const noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const TradedAmounts &tradedAmounts);

  string str() const;

  MonetaryAmount from;  // In currency of 'from' amount
  MonetaryAmount to;    // In the opposite currency
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::TradedAmounts> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    const auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::TradedAmounts &a, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{} -> {}", a.from, a.to);
  }
};
#endif