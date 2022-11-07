#pragma once

#include "cct_flatset.hpp"
#include "monetaryamount.hpp"

namespace cct {

struct CompareByCurrencyCode {
  bool operator()(MonetaryAmount lhs, MonetaryAmount rhs) const { return lhs.currencyCode() < rhs.currencyCode(); }
};

using MonetaryAmountByCurrencySet = FlatSet<MonetaryAmount, CompareByCurrencyCode>;
}  // namespace cct