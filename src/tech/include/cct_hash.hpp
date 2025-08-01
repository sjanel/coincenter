#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <tuple>

#include "cct_config.hpp"

namespace cct {
constexpr uint64_t HashValue64(uint64_t h1) {
  // Murmur-inspired hashing.
  CCT_STATIC_CONSTEXPR_IN_CONSTEXPR_FUNC uint64_t kMul = 0x9ddfea08eb382d69ULL;
  h1 *= kMul;
  h1 ^= (h1 >> 44);
  h1 *= kMul;
  h1 ^= (h1 >> 41);
  h1 *= kMul;
  return h1;
}

constexpr std::size_t HashCombine(std::size_t h1, std::size_t h2) {
  // Taken from boost::hash_combine
  static_assert(sizeof(std::size_t) == 4 || sizeof(std::size_t) == 8, "HashCombine not defined for this std::size_t");

  if constexpr (sizeof(std::size_t) == 4) {
    h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
  } else {
    // see https://github.com/HowardHinnant/hash_append/issues/7
    h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 12) + (h1 >> 4);
  }

  return h1;
}

class HashTuple {
 public:
  template <class Tuple>
  std::size_t operator()(const Tuple& tuple) const {
    return std::hash<std::size_t>()(std::apply([](const auto&... xs) { return (Component{xs}, ..., 0); }, tuple));
  }

 private:
  template <class T>
  struct Component {
    const T& value;

    std::size_t operator,(std::size_t n) const { return HashCombine(std::hash<T>()(value), n); }
  };
};

}  // namespace cct
