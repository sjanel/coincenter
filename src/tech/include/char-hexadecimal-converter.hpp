#pragma once

#include <concepts>
#include <type_traits>

namespace cct {

template <typename T>
concept signed_or_unsigned_char = std::same_as<T, char> || std::same_as<T, unsigned char>;

/// Writes to 'buf' the 2-char hexadecimal code of given char 'ch'.
/// Given buffer should have space for at least two chars.
/// Letters will be in lower case.
/// Return a pointer to the char immediately positioned after the written hexadecimal code.
/// Examples:
///  ',' -> "2c"
///  '?' -> "3f"
constexpr char *to_lower_hex(signed_or_unsigned_char auto ch, char *buf) {
  // TODO: can be static in C++23
  constexpr const char *const kHexits = "0123456789abcdef";

  buf[0] = kHexits[static_cast<unsigned char>(ch) >> 4U];
  buf[1] = kHexits[static_cast<unsigned char>(ch) & 0x0F];

  return buf + 2;
}

/// Writes to 'buf' the 2-char hexadecimal code of given char 'ch'.
/// Given buffer should have space for at least two chars.
/// Letters will be in upper case.
/// Return a pointer to the char immediately positioned after the written hexadecimal code.
/// Examples:
///  ',' -> "2C"
///  '?' -> "3F"
constexpr char *to_upper_hex(signed_or_unsigned_char auto ch, char *buf) {
  // TODO: can be static in C++23
  constexpr const char *const kHexits = "0123456789ABCDEF";

  buf[0] = kHexits[static_cast<unsigned char>(ch) >> 4U];
  buf[1] = kHexits[static_cast<unsigned char>(ch) & 0x0F];

  return buf + 2;
}

}  // namespace cct