#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "nchars.hpp"

namespace cct {

/// Concatenates variadic template std::string_view arguments at compile time and defines a std::string_view pointing on
/// a static storage. The storage is guaranteed to be null terminated (but not itself included in the returned value)
/// Adapted from
/// https://stackoverflow.com/questions/38955940/how-to-concatenate-static-strings-at-compile-time/62823211#62823211
template <std::string_view const&... Strs>
class JoinStringView {
 private:
  // Join all strings into a single std::array of chars
  static constexpr auto impl() noexcept {
    constexpr std::string_view::size_type len = (Strs.size() + ... + 0);
    std::array<char, len + 1U> arr;  // +1 for null terminated char
    if constexpr (len > 0) {
      auto append = [it = arr.begin()](auto const& s) mutable { it = std::copy(s.begin(), s.end(), it); };
      (append(Strs), ...);
    }
    arr.back() = '\0';
    return arr;
  }
  // Give the joined string static storage
  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.data(), arr.size() - 1};

  // c_str version (null-terminated)
  static constexpr const char* const c_str = arr.data();
};

// Helper to get the value out
template <std::string_view const&... Strs>
static constexpr auto JoinStringView_v = JoinStringView<Strs...>::value;

/// Same as JoinStringView but with a char separator between each string_view
template <std::string_view const& Sep, std::string_view const&... Strs>
class JoinStringViewWithSep {
 private:
  // Join all strings into a single std::array of chars
  static constexpr auto impl() noexcept {
    constexpr std::string_view::size_type len = (Strs.size() + ... + 0);
    constexpr auto nbSv = sizeof...(Strs);
    std::array<char, std::max(len + 1U + ((nbSv == 0U ? 0U : (nbSv - 1U)) * Sep.size()),
                              static_cast<std::string_view::size_type>(1))>
        arr;
    if constexpr (len > 0) {
      auto append = [it = arr.begin(), &arr](auto const& s) mutable {
        if (it != arr.begin()) {
          it = std::copy(Sep.begin(), Sep.end(), it);
        }
        it = std::copy(s.begin(), s.end(), it);
      };
      (append(Strs), ...);
    }
    arr.back() = '\0';
    return arr;
  }
  // Give the joined string static storage
  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.data(), arr.size() - 1};

  // c_str version (null-terminated)
  static constexpr const char* const c_str = arr.data();
};

// Helper to get the value out
template <std::string_view const& Sep, std::string_view const&... Strs>
static constexpr auto JoinStringViewWithSep_v = JoinStringViewWithSep<Sep, Strs...>::value;

namespace details {
template <std::string_view const& Sep, const auto& a, typename>
struct make_joined_string_view_impl;

template <std::string_view const& Sep, const auto& a, std::size_t... i>
struct make_joined_string_view_impl<Sep, a, std::index_sequence<i...>> {
  static constexpr auto value = JoinStringViewWithSep<Sep, a[i]...>::value;
};

}  // namespace details

// make joined string view from array like value
template <std::string_view const& Sep, const auto& a>
using make_joined_string_view = details::make_joined_string_view_impl<Sep, a, std::make_index_sequence<std::size(a)>>;

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
  static constexpr char ch = Char;

 public:
  static constexpr std::string_view value{&ch, 1};
};

template <char Char>
static constexpr auto CharToStringView_v = CharToStringView<Char>::value;

}  // namespace cct