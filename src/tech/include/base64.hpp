#pragma once

#include <algorithm>
#include <array>
#include <span>

#include "cct_string.hpp"

namespace cct {

namespace details {
inline void B64Encode(std::span<const char> binData, char *out, char *endOut) {
  int bitsCollected = 0;
  unsigned int accumulator = 0;

  static constexpr const char *const kB64Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  for (char ch : binData) {
    accumulator = (accumulator << 8) | (ch & 0xFFU);
    bitsCollected += 8;
    while (bitsCollected >= 6) {
      bitsCollected -= 6;
      *out = kB64Table[(accumulator >> bitsCollected) & 0x3FU];
      ++out;
    }
  }
  if (bitsCollected > 0) {
    accumulator <<= 6 - bitsCollected;
    *out = kB64Table[accumulator & 0x3FU];
    ++out;
  }

  std::fill(out, endOut, '=');
}

constexpr auto B64EncodedLen(auto binDataLen) { return static_cast<std::size_t>((binDataLen + 2) / 3) * 4; }

}  // namespace details

[[nodiscard]] inline string B64Encode(std::span<const char> binData) {
  string ret(details::B64EncodedLen(binData.size()), 0);
  details::B64Encode(binData, ret.data(), ret.data() + ret.size());
  return ret;
}
string B64Encode(const char *) = delete;

template <std::size_t N>
[[nodiscard]] auto B64Encode(const char (&binData)[N]) {
  std::array<char, details::B64EncodedLen(N)> ret;
  details::B64Encode(binData, ret.data(), ret.data() + ret.size());
  return ret;
}

template <std::size_t N>
[[nodiscard]] auto B64Encode(const std::array<char, N> &binData) {
  std::array<char, details::B64EncodedLen(N)> ret;
  details::B64Encode(binData, ret.data(), ret.data() + ret.size());
  return ret;
}

[[nodiscard]] string B64Decode(std::span<const char> ascData);
string B64Decode(const char *) = delete;

}  // namespace cct