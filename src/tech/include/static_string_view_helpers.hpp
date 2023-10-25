#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "mathhelpers.hpp"

namespace cct {

/// Concatenates variadic template std::string_view arguments at compile time and defines a std::string_view pointing on
/// a static storage. The storage is guaranteed to be null terminated.
/// Adapted from
/// https://stackoverflow.com/questions/38955940/how-to-concatenate-static-strings-at-compile-time/62823211#62823211
template <std::string_view const&... Strs>
class JoinStringView {
 private:
  // Join all strings into a single std::array of chars
  static constexpr auto impl() noexcept {
    constexpr std::size_t len = (Strs.size() + ... + 0);
    std::array<char, len + 1> arr{};
    if constexpr (len > 0) {
      auto append = [i = 0, &arr](auto const& s) mutable {
        for (auto c : s) arr[i++] = c;
      };
      (append(Strs), ...);
    }
    arr[len] = 0;
    return arr;
  }
  // Give the joined string static storage
  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.data(), arr.size() - 1};
};

// Helper to get the value out
template <std::string_view const&... Strs>
static constexpr auto JoinStringView_v = JoinStringView<Strs...>::value;

/// Converts an integer value to its string_view representation at compile time.
/// The underlying storage is not null terminated.
template <int64_t intVal>
class IntToStringView {
 private:
  static constexpr auto impl() noexcept {
    std::array<char, nchars(intVal)> arr;
    if constexpr (intVal == 0) {
      arr[0] = '0';
      return arr;
    }
    auto endIt = arr.end();
    int64_t val = intVal;
    if constexpr (intVal < 0) {
      arr[0] = '-';
      val = -val;
    }
    do {
      *--endIt = (val % 10) + '0';
      val /= 10;
    } while (val != 0);
    return arr;
  }

  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.begin(), arr.end()};
};

template <int64_t intVal>
static constexpr auto IntToStringView_v = IntToStringView<intVal>::value;

/// Creates a std::string_view on a storage with a single char available at compile time.
template <char Char>
class CharToStringView {
 private:
  static constexpr char c = Char;

 public:
  static constexpr std::string_view value{&c, 1};
};

template <char Char>
static constexpr auto CharToStringView_v = CharToStringView<Char>::value;

}  // namespace cct