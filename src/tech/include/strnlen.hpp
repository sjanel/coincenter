#pragma once

#include <cstddef>
#include <cstring>

namespace cct {

/**
 * @brief Returns the first position of the null-terminating char of given buffer, or maxLen if not found before this
 * position. This function is not standard, even if it exists for some compilers, so we define it here.
 */
inline std::size_t strnlen(const char *start, std::size_t maxLen) {
  const char *outPtr = static_cast<const char *>(std::memchr(start, 0, maxLen));

  if (outPtr == nullptr) {
    return maxLen;
  }

  return outPtr - start;
}

}  // namespace cct