#pragma once

#include <cstdint>

#include "cct_vector.hpp"
#include "currencycode.hpp"

namespace cct::schema {

struct FiatsCache {
  int64_t timeepoch{};
  vector<CurrencyCode> fiats;

  using trivially_relocatable = is_trivially_relocatable<vector<CurrencyCode>>::type;
};

}  // namespace cct::schema