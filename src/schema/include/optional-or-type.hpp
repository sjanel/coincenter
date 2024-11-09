#pragma once

#include <optional>

namespace cct::schema {

template <class T, bool Optional>
struct optional_or {
  using type = T;
};

template <class T>
struct optional_or<T, true> {
  using type = std::optional<T>;
};

template <class T, bool Optional>
using optional_or_t = optional_or<T, Optional>::type;

}  // namespace cct::schema