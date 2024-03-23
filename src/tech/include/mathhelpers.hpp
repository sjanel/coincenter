#pragma once

#include <array>
#include <concepts>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace cct {
/// constexpr and integral version of math.power.
/// Taken from https://gist.github.com/orlp/3551590
constexpr int64_t ipow(int64_t base, uint8_t exp) noexcept {
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

/// Optimization of ipow(int64_t base, 10)
constexpr int64_t ipow10(uint8_t exp) noexcept {
  constexpr const int64_t kPow10Table[] = {1LL,
                                           10LL,
                                           100LL,
                                           1000LL,
                                           10000LL,
                                           100000LL,
                                           1000000LL,
                                           10000000LL,
                                           100000000LL,
                                           1000000000LL,
                                           10000000000LL,
                                           100000000000LL,
                                           1000000000000LL,
                                           10000000000000LL,
                                           100000000000000LL,
                                           1000000000000000LL,
                                           10000000000000000LL,
                                           100000000000000000LL,
                                           1000000000000000000LL};
  return exp < std::size(kPow10Table) ? kPow10Table[exp] : std::numeric_limits<int64_t>::max();
}

/// Return the number of digits of given integral.
/// The minus sign is not counted.
/// Uses dichotomy for highest performance as possible.
constexpr int ndigits(std::signed_integral auto n) noexcept {
  using T = decltype(n);
  if constexpr (std::is_same_v<T, int8_t>) {
    return n < 0 ? (n > -100 ? (n > -10 ? 1 : 2) : 3) : (n < 100 ? (n < 10 ? 1 : 2) : 3);
  } else if constexpr (std::is_same_v<T, int16_t>) {
    return n < 0 ? (n > -1000 ? (n > -100 ? (n > -10 ? 1 : 2) : 3) : (n > -10000 ? 4 : 5))
                 : (n < 1000 ? (n < 100 ? (n < 10 ? 1 : 2) : 3) : (n < 10000 ? 4 : 5));
  } else if constexpr (std::is_same_v<T, int32_t>) {
    return n < 0 ? (n > -1000000
                        ? (n > -1000 ? (n > -100 ? (n > -10 ? 1 : 2) : 3) : (n > -100000 ? (n > -10000 ? 4 : 5) : 6))
                        : (n > -100000000 ? (n > -10000000 ? 7 : 8) : (n > -1000000000 ? 9 : 10)))
                 : (n < 1000000 ? (n < 1000 ? (n < 100 ? (n < 10 ? 1 : 2) : 3) : (n < 100000 ? (n < 10000 ? 4 : 5) : 6))
                                : (n < 100000000 ? (n < 10000000 ? 7 : 8) : (n < 1000000000 ? 9 : 10)));
  } else {
    // int64_t
    return n < 0 ? (n > -10000000000L
                        ? (n > -1000000L ? (n > -1000L ? (n > -100L ? (n > -10L ? 1 : 2) : 3)
                                                       : (n > -100000L ? (n > -10000L ? 4 : 5) : 6))
                                         : (n > -100000000L ? (n > -10000000L ? 7 : 8) : (n > -1000000000L ? 9 : 10)))
                        : (n > -10000000000000000L
                               ? (n > -10000000000000L
                                      ? (n > -1000000000000L ? (n > -100000000000L ? 11 : 12) : 13)
                                      : (n > -1000000000000000L ? (n > -100000000000000L ? 14 : 15) : 16))
                               : (n > -1000000000000000000L ? (n > -100000000000000000L ? 17 : 18) : 19)))
                 : (n < 10000000000L
                        ? (n < 1000000L ? (n < 1000L ? (n < 100L ? (n < 10L ? 1 : 2) : 3)
                                                     : (n < 100000L ? (n < 10000L ? 4 : 5) : 6))
                                        : (n < 100000000L ? (n < 10000000L ? 7 : 8) : (n < 1000000000L ? 9 : 10)))
                        : (n < 10000000000000000L
                               ? (n < 10000000000000L ? (n < 1000000000000L ? (n < 100000000000L ? 11 : 12) : 13)
                                                      : (n < 1000000000000000L ? (n < 100000000000000L ? 14 : 15) : 16))
                               : (n < 1000000000000000000L ? (n < 100000000000000000L ? 17 : 18) : 19)));
  }
}

/// Count the number of digits including the possible minus sign for negative integrals.
constexpr int nchars(std::signed_integral auto n) noexcept { return ndigits(n) + static_cast<int>(n < 0); }

/// Return the number of digits of given integral.
/// Uses dichotomy for highest performance as possible.
constexpr int ndigits(std::unsigned_integral auto n) noexcept {
  using T = decltype(n);
  if constexpr (std::is_same_v<T, uint8_t>) {
    return n < 100U ? (n < 10U ? 1 : 2) : 3;
  } else if constexpr (std::is_same_v<T, uint16_t>) {
    return n < 1000U ? (n < 100U ? (n < 10U ? 1 : 2) : 3) : (n < 10000U ? 4 : 5);
  } else if constexpr (std::is_same_v<T, uint32_t>) {
    return n < 1000000U ? (n < 1000U ? (n < 100U ? (n < 10U ? 1 : 2) : 3) : (n < 100000U ? (n < 10000U ? 4 : 5) : 6))
                        : (n < 100000000U ? (n < 10000000U ? 7 : 8) : (n < 1000000000U ? 9 : 10));
  } else {
    // uint64_t
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
}

/// Synonym of ndigits for unsigned types.
constexpr int nchars(std::unsigned_integral auto n) noexcept { return ndigits(n); }

}  // namespace cct