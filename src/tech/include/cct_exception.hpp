#pragma once

#include <cstring>
#include <exception>

#include "cct_format.hpp"

namespace cct {

/// Non allocating basic exception class that can be constructed from a string literal or a format string (truncated).
class exception : public std::exception {
 public:
  static constexpr int kMsgMaxLen = 87;

  template <int N>
  explicit exception(const char (&str)[N]) noexcept
    requires(N <= kMsgMaxLen)
  {
    std::memcpy(_data, str, N);
    _data[N] = '\0';
  }

  template <typename... Args>
  explicit exception(format_string<Args...> fmt, Args&&... args) {
    auto sz = cct::format_to_n(_data, kMsgMaxLen, fmt, std::forward<Args>(args)...).size;
    if (sz > kMsgMaxLen) {
      std::memcpy(_data + kMsgMaxLen - 3, "...", 3);
      sz = kMsgMaxLen;
    }
    _data[sz] = '\0';
  }

  [[nodiscard]] const char* what() const noexcept override { return _data; }

 private:
  char _data[kMsgMaxLen + 1];
};

}  // namespace cct
