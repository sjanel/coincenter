#pragma once

#include "cct_config.hpp"

namespace cct {

[[noreturn]] CCT_ALWAYS_INLINE void unreachable() {
#if defined(__GNUC__)
  __builtin_unreachable();
#elif defined(CCT_MSVC)
  __assume(0);
#else
#error "To be implemented"
#endif
}

}  // namespace cct