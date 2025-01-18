#pragma once

#include "monetaryamount.hpp"

namespace cct {

struct AmountPrice {
  bool operator==(const AmountPrice &) const noexcept = default;

  MonetaryAmount amount;
  MonetaryAmount price;
};

}  // namespace cct