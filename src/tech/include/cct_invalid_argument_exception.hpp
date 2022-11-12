#pragma once

#include "cct_exception.hpp"
#include "cct_format.hpp"

namespace cct {
class invalid_argument : public exception {
 public:
  template <unsigned N, std::enable_if_t<N <= kMsgMaxLen + 1, bool> = true>
  explicit invalid_argument(const char (&str)[N]) noexcept : exception(str) {}

  explicit invalid_argument(string&& str) noexcept : exception(std::move(str)) {}

#ifdef CCT_MSVC
  // MSVC bug: https://developercommunity.visualstudio.com/t/using-fmtlib-on-a-custom-exceptions-constructor-pa/1673659
  // do not use fmt for building an exception waiting for the bug to be fixed...
  // Exception message will be incorrect.
  template <typename... Args>
  explicit invalid_argument(std::string_view fmt, Args&&... args) : exception(fmt, std::forward<Args>(args)...) {}
#else
  template <typename... Args>
  explicit invalid_argument(format_string<Args...> fmt, Args&&... args) : exception(fmt, std::forward<Args>(args)...) {}
#endif
};
}  // namespace cct