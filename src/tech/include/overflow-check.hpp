#pragma once

#include <concepts>
#include <limits>
#include <type_traits>

namespace cct {

/// Simple utility function to check if performing the operator lhs + rhs (as signed integrals) would trigger an
/// overflow.
template <std::signed_integral T>
constexpr bool WillSumOverflow(T lhs, T rhs) {
  if ((static_cast<T>(lhs) ^ static_cast<T>(rhs)) < 0) {
    return false;
  }
  if (lhs > 0) {
    return static_cast<T>(rhs) > std::numeric_limits<T>::max() - static_cast<T>(lhs);
  }
  return static_cast<T>(rhs) < std::numeric_limits<T>::min() - static_cast<T>(lhs);
}

}  // namespace cct