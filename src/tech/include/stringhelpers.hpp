#pragma once

#include <charconv>
#include <concepts>
#include <cstring>
#include <string_view>
#include <system_error>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "mathhelpers.hpp"

namespace cct {

namespace details {
template <class SizeType>
void ToChars(char *first, SizeType s, std::integral auto i) {
  if (auto [ptr, errc] = std::to_chars(first, first + s, i); CCT_UNLIKELY(errc != std::errc())) {
    throw exception("Unable to decode integral into string");
  }
}
}  // namespace details

inline string ToString(std::integral auto val) {
  const int nbDigitsInt = nchars(val);
  string s(nbDigitsInt, '0');
  details::ToChars(s.data(), nbDigitsInt, val);
  return s;
}

template <std::integral Integral>
Integral FromString(std::string_view str) {
  Integral ret{};
  if (auto [ptr, errc] = std::from_chars(str.data(), str.data() + str.size(), ret); CCT_UNLIKELY(errc != std::errc())) {
    if (errc == std::errc::result_out_of_range) {
      throw exception("'{}' would produce an out of range integral", str);
    }
    throw exception("Unable to decode '{}' into integral", str);
  }
  return ret;
}

template <class StringType>
void SetString(StringType &s, std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  s.resize(nbDigitsInt, '0');
  details::ToChars(s.data(), nbDigitsInt, i);
}

template <class StringType>
void AppendString(StringType &s, std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  s.append(nbDigitsInt, '0');
  details::ToChars(s.data() + static_cast<int>(s.size()) - nbDigitsInt, nbDigitsInt, i);
}

inline std::size_t strnlen(const char *start, std::size_t maxLen) {
  const void *outPtr = memchr(start, 0, maxLen);
  if (outPtr == nullptr) {
    return maxLen;
  }
  return static_cast<const char *>(outPtr) - start;
}

}  // namespace cct