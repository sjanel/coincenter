#pragma once

#include <algorithm>
#include <array>
#include <exception>
#include <iterator>
#include <variant>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

/// Implementation of a simple string (untruncated) storage with noexcept constructors and what() method.
/// To be constructed from a 'const char []', it needs to be small enough to fill in the buffer. For custom, longer
/// error messages, the constructor from the rvalue of a string should be used instead.
class exception : public std::exception {
 public:
  static constexpr int kMsgMaxLen = 80;

  template <unsigned N, std::enable_if_t<N <= kMsgMaxLen + 1, bool> = true>
  explicit exception(const char (&str)[N]) noexcept {
    // Hint: default constructor constructs a variant holding the value-initialized value of the first alternative
    // (index() is zero). In our case, it's a std::array, which is what we want here.

    std::ranges::copy(str, std::get<0>(_data).data());
    // No need to set the null terminating char - it has already been set to 0 thanks to value initialization of the
    // char array.
  }

  explicit exception(string&& str) noexcept(std::is_nothrow_move_constructible_v<string>) : _data(std::move(str)) {}

  template <typename... Args>
  explicit exception(format_string<Args...> fmt, Args&&... args) : _data(std::in_place_type<string>) {
    format_to(std::back_inserter(std::get<1>(_data)), fmt, std::forward<Args>(args)...);
  }

  exception(const exception& o) = delete;
  exception(exception&&) noexcept = default;
  exception& operator=(const exception& o) = delete;
  exception& operator=(exception&&) noexcept = default;

  const char* what() const noexcept override {
    switch (_data.index()) {
      case 0:
        return std::get<0>(_data).data();
      case 1:
        return std::get<1>(_data).c_str();
      default:
        unreachable();
    }
  }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  using CharStorage = std::array<char, kMsgMaxLen + 1>;

  std::variant<CharStorage, string> _data;
};
}  // namespace cct
