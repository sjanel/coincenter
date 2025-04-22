#pragma once

#include <concepts>
#include <limits>

namespace cct {

/// Simple utility function to check if performing the operator lhs + rhs (as signed integrals) would trigger an
/// overflow.
template <std::signed_integral T>
constexpr bool WillSumOverflow(T lhs, T rhs) {
  if ((lhs ^ rhs) < 0) {
    return false;
  }
  if (lhs > 0) {
    return rhs > std::numeric_limits<T>::max() - lhs;
  }
  return rhs < std::numeric_limits<T>::min() - lhs;
}

}  // namespace cct