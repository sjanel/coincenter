

#pragma once

#include <cstdint>
#include <functional>
#include <tuple>
#include <utility>

namespace cct {
inline std::size_t HashValue64(std::size_t x) {
  // Murmur-inspired hashing.
  const std::size_t kMul = 0x9ddfea08eb382d69ULL;
  std::size_t b = x * kMul;
  b ^= (b >> 44);
  b *= kMul;
  b ^= (b >> 41);
  b *= kMul;
  return b;
}

inline std::size_t HashCombine(std::size_t h1, std::size_t h2) {
  // Taken from boost::hash_combine
  h1 ^= h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2);
  return h1;
}

// recursive variadic template hash
template <class T>
std::size_t HashVar(const T& v) {
  return std::hash<T>()(v);
}

template <class T, class... Args>
std::size_t HashVar(T first, Args... args) {
  return HashCombine(std::hash<T>()(first), HashVar(args...));
}

}  // namespace cct

namespace std {
template <class... TT>
struct hash<std::tuple<TT...>> {
  size_t operator()(const std::tuple<TT...>& tup) const {
    auto lazyHasher = [](size_t h, auto&&... values) {
      auto lazyCombiner = [&h](auto&& val) {
        h ^= std::hash<std::decay_t<decltype(val)>>{}(val) + 0Xeeffddcc + (h << 5) + (h >> 3);
      };
      (void)lazyCombiner;
      (lazyCombiner(std::forward<decltype(values)>(values)), ...);
      return h;
    };
    return std::apply(lazyHasher, std::tuple_cat(std::tuple(0), tup));
  }
};
}  // namespace std
