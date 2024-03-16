#pragma once

#include "monetaryamount.hpp"

namespace cct {

struct AmountPrice {
  MonetaryAmount amount;
  MonetaryAmount price;
};

}  // namespace cct