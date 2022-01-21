#pragma once

#include "cct_exception.hpp"

namespace cct {
class invalid_argument : public exception {
 public:
  template <unsigned N, std::enable_if_t<N <= kMsgMaxLen + 1, bool> = true>
  explicit invalid_argument(const char (&str)[N]) noexcept : exception(str) {}

  explicit invalid_argument(string&& str) noexcept : exception(std::move(str)) {}
};
}  // namespace cct