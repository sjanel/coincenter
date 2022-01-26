#pragma once

#include "cct_string.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

struct TradedAmounts {
  TradedAmounts() noexcept(std::is_nothrow_default_constructible_v<MonetaryAmount>) = default;

  TradedAmounts(CurrencyCode fromCurrencyCode, CurrencyCode toCurrencyCode)
      : tradedFrom(0, fromCurrencyCode), tradedTo(0, toCurrencyCode) {}

  TradedAmounts(MonetaryAmount fromAmount, MonetaryAmount toAmount) : tradedFrom(fromAmount), tradedTo(toAmount) {}

  TradedAmounts operator+(const TradedAmounts &o) const;
  TradedAmounts &operator+=(const TradedAmounts &o) {
    *this = *this + o;
    return *this;
  }

  bool operator==(const TradedAmounts &) const = default;

  bool isZero() const { return tradedFrom.isZero() && tradedTo.isZero(); }

  string str() const;

  MonetaryAmount tradedFrom;  // In currency of 'from' amount
  MonetaryAmount tradedTo;    // In the opposite currency
};

}  // namespace cct