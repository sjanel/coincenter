#pragma once

#include <algorithm>
#include <cstdint>

#include "cct_fixedcapacityvector.hpp"

namespace cct {

constexpr int nb_bytes_utf8(uint32_t cp) {
  if (cp <= 0x007F) {
    return 1;
  }
  if (cp <= 0x07FF) {
    return 2;
  }
  if (cp <= 0xFFFF) {
    return 3;
  }
  if (cp <= 0x10FFFF) {
    return 4;
  }
  // invalid, assume 1
  return 1;
}

inline auto to_utf8_vector(uint32_t cp) {
  const int count = nb_bytes_utf8(cp);

  FixedCapacityVector<char, 4> result(count);

  if (count > 1) {
    for (int pos = count - 1; pos > 0; --pos) {
      result[pos] = static_cast<char>(0x80 | (cp & 0x3F));
      cp >>= 6;
    }

    for (int pos = 0; pos < count; ++pos) {
      cp |= (1 << (7 - pos));
    }
  }

  result[0] = static_cast<char>(cp);

  return result;
}

/**
 * Decode in place char buffer with literal unicode characters like "\\u10348" into utf8 characters.
 * @param first Start of buffer
 * @param last End of buffer
 * @return Pointer to the end of the decoded buffer
 */
inline char *decode_utf8(char *first, const char *last) {
  char *dst = first;
  while (first < last) {
    if (*first == '\\' && (first + 1) < last && *(first + 1) == 'u' && (first + 6) <= last) {
      uint32_t cp = 0;
      first += 2;
      for (auto endIt = first + 4; first != endIt; ++first) {
        char ch = *first;
        cp <<= 4;
        if (ch >= 'a') {
          cp |= (ch - 'a' + 10);
        } else if (ch >= 'A') {
          cp |= (ch - 'A' + 10);
        } else {
          cp |= (ch - '0');
        }
      }
      dst = std::ranges::copy(to_utf8_vector(cp), dst).out;
    } else {
      *dst++ = *first++;
    }
  }
  return dst;
}

/**
 * @brief Same as decode_utf8(char*, char*) but for string-like (char based containers) objects.
 * The string is resized and modified in place. It does not allocate memory as the input string can only shrink.
 */
template <class StringLike>
void decode_utf8(StringLike &str) {
  str.resize(decode_utf8(str.data(), str.data() + str.size()) - str.data());
}

}  // namespace cct