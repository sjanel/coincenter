#pragma once

#include <cstdint>

#include "timedef.hpp"

namespace cct {

struct QueryRetryPolicy {
  enum class TooManyFailuresPolicy : int8_t { kReturnEmpty, kThrowException };

  Duration initialRetryDelay{milliseconds(500)};
  float exponentialBackoff{2};
  int16_t nbMaxRetries{5};
  TooManyFailuresPolicy tooManyFailuresPolicy{TooManyFailuresPolicy::kReturnEmpty};
};

}  // namespace cct