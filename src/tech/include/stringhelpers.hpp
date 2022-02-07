#pragma once

#include <charconv>
#include <concepts>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_string.hpp"
#include "mathhelpers.hpp"

namespace cct {

namespace details {
template <class SizeType>
inline void ToChars(char *first, SizeType s, std::integral auto i) {
  if (auto [ptr, errc] = std::to_chars(first, first + s, i); CCT_UNLIKELY(errc != std::errc())) {
    throw exception("Unable to decode integral in string");
  }
}
}  // namespace details

string ToString(std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  string s(nbDigitsInt, '0');
  details::ToChars(s.data(), nbDigitsInt, i);
  return s;
}

template <std::integral Integral>
Integral FromString(std::string_view str) {
  Integral ret;
  if (auto [ptr, errc] = std::from_chars(str.data(), str.data() + str.size(), ret); CCT_UNLIKELY(errc != std::errc())) {
    throw exception("Unable to decode string in integral");
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

}  // namespace cct