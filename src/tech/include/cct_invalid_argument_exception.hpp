#pragma once

#include <utility>

#include "cct_exception.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"

namespace cct {
class invalid_argument : public exception {
 public:
  template <unsigned N, std::enable_if_t<N <= kMsgMaxLen + 1, bool> = true>
  explicit invalid_argument(const char (&str)[N]) noexcept : exception(str) {}

  explicit invalid_argument(string&& str) noexcept : exception(std::move(str)) {}

  template <typename... Args>
  explicit invalid_argument(format_string<Args...> fmt, Args&&... args) : exception(fmt, std::forward<Args>(args)...) {}
};
}  // namespace cct