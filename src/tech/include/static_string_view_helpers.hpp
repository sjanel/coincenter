#pragma once

#include <algorithm>
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
    std::array<char, len + 1U> a;  // +1 for null terminated char
    if constexpr (len > 0) {
      auto append = [it = a.begin()](auto const& s) mutable { it = std::ranges::copy(s, it).out; };
      (append(Strs), ...);
    }
    a.back() = '\0';
    return a;
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
inline constexpr auto JoinStringView_v = JoinStringView<Strs...>::value;

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
        a;
    if constexpr (len > 0) {
      auto append = [it = a.begin(), &a](auto const& s) mutable {
        if (it != a.begin()) {
          it = std::ranges::copy(Sep, it).out;
        }
        it = std::ranges::copy(s, it).out;
      };
      (append(Strs), ...);
    }
    a.back() = '\0';
    return a;
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
inline constexpr auto JoinStringViewWithSep_v = JoinStringViewWithSep<Sep, Strs...>::value;

namespace details {
/// Same as JoinStringViewWithSep, but joining the elements of an array like value.
/// Its elements are iterated in a constexpr function instead of being expanded as reference template arguments
/// (references to array elements as non-type template arguments trigger an internal compiler error with MSVC 19.51).
template <std::string_view const& Sep, const auto& a>
class JoinArrayStringViewWithSep {
 private:
  static constexpr auto impl() noexcept {
    constexpr std::string_view::size_type len = []() {
      std::string_view::size_type totalLen = 0;
      for (std::string_view sv : a) {
        totalLen += sv.size();
      }
      return totalLen;
    }();
    constexpr auto nbSv = std::size(a);
    std::array<char, len + 1U + ((nbSv == 0U ? 0U : (nbSv - 1U)) * Sep.size())> joined{};
    auto it = joined.begin();
    for (std::size_t svPos = 0; svPos < nbSv; ++svPos) {
      if (svPos != 0) {
        it = std::ranges::copy(Sep, it).out;
      }
      it = std::ranges::copy(std::string_view(a[svPos]), it).out;
    }
    joined.back() = '\0';
    return joined;
  }

  // Give the joined string static storage
  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.data(), arr.size() - 1};
};

}  // namespace details

// make joined string view from array like value
template <std::string_view const& Sep, const auto& a>
using make_joined_string_view = details::JoinArrayStringViewWithSep<Sep, a>;

/// Converts an integer value to its string_view representation at compile time.
/// The underlying storage is not null terminated.
template <int64_t intVal>
class IntToStringView {
 private:
  static constexpr auto impl() noexcept {
    std::array<char, nchars(intVal)> a;
    if constexpr (intVal == 0) {
      a[0] = '0';
      return a;
    }
    auto endIt = a.end();
    int64_t val = intVal;
    if constexpr (intVal < 0) {
      a[0] = '-';
      val = -val;
    }
    do {
      *--endIt = (val % 10) + '0';
      val /= 10;
    } while (val != 0);
    return a;
  }

  static constexpr auto arr = impl();

 public:
  // View as a std::string_view
  static constexpr std::string_view value{arr.begin(), arr.end()};
};

template <int64_t intVal>
inline constexpr auto IntToStringView_v = IntToStringView<intVal>::value;

/// Creates a std::string_view on a storage with a single char available at compile time.
template <char Char>
class CharToStringView {
 private:
  static constexpr char ch = Char;

 public:
  static constexpr std::string_view value{&ch, 1};
};

template <char Char>
inline constexpr auto CharToStringView_v = CharToStringView<Char>::value;

}  // namespace cct