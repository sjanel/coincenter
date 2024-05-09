#pragma once

#include <concepts>

#include "ndigits.hpp"

namespace cct {

/// Count the number of digits including the possible minus sign for negative integrals.
constexpr int nchars(std::signed_integral auto n) noexcept { return ndigits(n) + static_cast<int>(n < 0); }

/// Synonym of ndigits for unsigned types.
constexpr int nchars(std::unsigned_integral auto n) noexcept { return ndigits(n); }

}  // namespace cct