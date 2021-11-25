#pragma once

#include <charconv>
#include <concepts>

#include "cct_mathhelpers.hpp"

namespace cct {

namespace details {
template <class SizeType>
inline void ToChars(char *last, SizeType s, std::integral auto i) {
  std::to_chars(last - s, last, i);
}
}  // namespace details

template <class StringType>
StringType ToString(std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  StringType s(nbDigitsInt, '0');
  details::ToChars(s.data() + nbDigitsInt, nbDigitsInt, i);
  return s;
}

template <class StringType>
void SetChars(StringType &s, std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  s.resize(nbDigitsInt, '0');
  details::ToChars(s.data() + nbDigitsInt, nbDigitsInt, i);
}

template <class StringType>
void AppendChars(StringType &s, std::integral auto i) {
  const int nbDigitsInt = nchars(i);
  s.append(nbDigitsInt, '0');
  details::ToChars(s.data() + s.size(), nbDigitsInt, i);
}

}  // namespace cct