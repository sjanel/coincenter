#pragma once

#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

#include "cct_string.hpp"

namespace cct {

namespace details {
inline void B64EncodeImpl(std::span<const char> binData, char *out, char *endOut) {
  int bitsCollected{};
  uint32_t accumulator{};

  static constexpr const char kB64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  static constexpr auto kB64NbBits = 6;
  static constexpr decltype(accumulator) kMask6 = (1U << kB64NbBits) - 1U;

  for (char ch : binData) {
    accumulator = (accumulator << CHAR_BIT) | static_cast<uint8_t>(ch);
    bitsCollected += CHAR_BIT;
    while (bitsCollected >= kB64NbBits) {
      bitsCollected -= kB64NbBits;
      *out++ = kB64Table[(accumulator >> bitsCollected) & kMask6];
    }
  }
  if (bitsCollected > 0) {
    accumulator <<= kB64NbBits - bitsCollected;
    *out++ = kB64Table[accumulator & kMask6];
  }

  std::fill(out, endOut, '=');
}

constexpr auto B64EncodedLen(auto binDataLen) { return static_cast<std::size_t>((binDataLen + 2) / 3) * 4; }

}  // namespace details

[[nodiscard]] inline string B64Encode(std::span<const char> binData) {
  string ret(details::B64EncodedLen(binData.size()), 0);
  details::B64EncodeImpl(binData, ret.data(), ret.data() + ret.size());
  return ret;
}
string B64Encode(const char *) = delete;

template <std::size_t N>
[[nodiscard]] auto B64Encode(const char (&binData)[N]) {
  std::array<char, details::B64EncodedLen(N)> ret;
  details::B64EncodeImpl(binData, ret.data(), ret.data() + ret.size());
  return ret;
}

template <std::size_t N>
[[nodiscard]] auto B64Encode(const std::array<char, N> &binData) {
  std::array<char, details::B64EncodedLen(N)> ret;
  details::B64EncodeImpl(binData, ret.data(), ret.data() + ret.size());
  return ret;
}

[[nodiscard]] string B64Decode(std::span<const char> ascData);
string B64Decode(const char *) = delete;

}  // namespace cct
