#pragma once

#include "cct_exception.hpp"
#include "cct_format.hpp"

namespace cct {

class invalid_argument : public exception {
 public:
  template <int N>
  explicit invalid_argument(const char (&str)[N]) noexcept
    requires(N <= kMsgMaxLen + 1)
      : exception(str) {}

  template <typename... Args>
  explicit invalid_argument(format_string<Args...> fmt, Args&&... args) : exception(fmt, std::forward<Args>(args)...) {}
};

}  // namespace cct