#pragma once

#include <cstdint>
#include <limits>

namespace cct {
struct VolAndPriNbDecimals {
  VolAndPriNbDecimals() : volNbDecimals(std::numeric_limits<uintmax_t>::digits10), priNbDecimals(volNbDecimals) {}

  VolAndPriNbDecimals(int8_t volNbDec, int8_t priNbDec) : volNbDecimals(volNbDec), priNbDecimals(priNbDec) {}

  bool operator==(VolAndPriNbDecimals o) const {
    return volNbDecimals == o.volNbDecimals && priNbDecimals == o.priNbDecimals;
  }
  bool operator!=(VolAndPriNbDecimals o) const { return !(*this == o); }

  int8_t volNbDecimals;
  int8_t priNbDecimals;
};
}  // namespace cct