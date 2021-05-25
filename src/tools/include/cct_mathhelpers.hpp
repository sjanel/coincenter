#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include "cct_config.hpp"

namespace cct {
/// constexpr and integral version of math.power.
/// Taken from https://gist.github.com/orlp/3551590
constexpr int64_t ipow(int64_t base, uint8_t exp) {
  constexpr uint8_t highest_bit_set[] = {0,   1,   2,   2,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,
                                         4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,
                                         5,   5,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,
                                         6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,   6,
                                         6,   6,   6,   255,  // anything past 63 is a guaranteed overflow with base > 1
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255,
                                         255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

  int64_t result = 1;

  switch (highest_bit_set[exp]) {
    case 255:  // we use 255 as an overflow marker and return 0 on overflow/underflow
      return base == 1 ? 1 : (base == -1 ? (1 - 2 * (exp & 1)) : 0);
    case 6:
      if (exp & 1) result *= base;
      exp >>= 1;
      base *= base;
      [[fallthrough]];
    case 5:
      if (exp & 1) result *= base;
      exp >>= 1;
      base *= base;
      [[fallthrough]];
    case 4:
      if (exp & 1) result *= base;
      exp >>= 1;
      base *= base;
      [[fallthrough]];
    case 3:
      if (exp & 1) result *= base;
      exp >>= 1;
      base *= base;
      [[fallthrough]];
    case 2:
      if (exp & 1) result *= base;
      exp >>= 1;
      base *= base;
      [[fallthrough]];
    case 1:
      if (exp & 1) result *= base;
      [[fallthrough]];
    default:
      return result;
  }
}

/// Return the number of digits of given integral.
/// Uses dichotomy for highest performance as possible.
constexpr int ndigits(int32_t n) {
  if (n < 0) {
    if (CCT_UNLIKELY(n == std::numeric_limits<decltype(n)>::min())) {
      ++n;
    }
    n *= -1;
  }
  return n < 1000000 ? (n < 1000 ? (n < 100 ? (n < 10 ? 1 : 2) : 3) : (n < 100000 ? (n < 10000 ? 4 : 5) : 6))
                     : (n < 100000000 ? (n < 10000000 ? 7 : 8) : (n < 1000000000 ? 9 : 10));
}

/// Return the number of digits of given integral.
/// Uses dichotomy for highest performance as possible.
constexpr int ndigits(uint64_t n) {
  return n < 10000000000UL
             ? (n < 1000000UL
                    ? (n < 1000UL ? (n < 100UL ? (n < 10UL ? 1 : 2) : 3) : (n < 100000UL ? (n < 10000UL ? 4 : 5) : 6))
                    : (n < 100000000UL ? (n < 10000000UL ? 7 : 8) : (n < 1000000000UL ? 9 : 10)))
             : (n < 10000000000000000UL
                    ? (n < 10000000000000UL ? (n < 1000000000000UL ? (n < 100000000000UL ? 11 : 12) : 13)
                                            : (n < 1000000000000000UL ? (n < 100000000000000UL ? 14 : 15) : 16))
                    : (n < 1000000000000000000UL ? (n < 100000000000000000UL ? 17 : 18)
                                                 : (n < 10000000000000000000UL ? 19 : 20)));
}

/// Return the number of digits of given integral.
/// Uses dichotomy for highest performance as possible.
constexpr int ndigits(int64_t n) {
  if (n < 0) {
    if (CCT_UNLIKELY(n == std::numeric_limits<decltype(n)>::min())) {
      ++n;
    }
    n *= -1;
  }
  return ndigits(static_cast<uint64_t>(n));
}
}  // namespace cct