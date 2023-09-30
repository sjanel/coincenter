

#pragma once

#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>

namespace cct {
constexpr uint64_t HashValue64(uint64_t h1) {
  // Murmur-inspired hashing.
  constexpr uint64_t kMul = 0x9ddfea08eb382d69ULL;
  h1 *= kMul;
  h1 ^= (h1 >> 44);
  h1 *= kMul;
  h1 ^= (h1 >> 41);
  h1 *= kMul;
  return h1;
}

constexpr size_t HashCombine(size_t h1, size_t h2) {
  // Taken from boost::hash_combine
  static_assert(sizeof(size_t) == 4 || sizeof(size_t) == 8, "HashCombine not defined for this size_t");
  if constexpr (sizeof(size_t) == 4) {
    h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
  } else if constexpr (sizeof(size_t) == 8) {
    // see https://github.com/HowardHinnant/hash_append/issues/7
    h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 12) + (h1 >> 4);
  }
  return h1;
}

class HashTuple {
 public:
  template <class Tuple>
  size_t operator()(const Tuple& tuple) const {
    return std::hash<size_t>()(std::apply([](const auto&... xs) { return (Component{xs}, ..., 0); }, tuple));
  }

 private:
  template <class T>
  struct Component {
    const T& value;

    size_t operator,(size_t n) const { return HashCombine(std::hash<T>()(value), n); }
  };
};

}  // namespace cct
