#pragma once

#include <cstdint>
#include <limits>

namespace cct {
struct VolAndPriNbDecimals {
  constexpr VolAndPriNbDecimals() noexcept = default;

  constexpr VolAndPriNbDecimals(int8_t volNbDec, int8_t priNbDec) : volNbDecimals(volNbDec), priNbDecimals(priNbDec) {}

  constexpr bool operator==(const VolAndPriNbDecimals &o) const = default;

  int8_t volNbDecimals = std::numeric_limits<uintmax_t>::digits10;
  int8_t priNbDecimals = std::numeric_limits<uintmax_t>::digits10;
};
}  // namespace cct