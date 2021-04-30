
#include "cct_nonce.hpp"

#include <chrono>

namespace cct {

Nonce Nonce_TimeSinceEpoch() {
  Nonce ret;
  const auto p1 = std::chrono::system_clock::now();
  uintmax_t msSinceEpoch = std::chrono::duration_cast<std::chrono::milliseconds>(p1.time_since_epoch()).count();
  return std::to_string(msSinceEpoch);
}

}  // namespace cct
