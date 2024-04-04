#pragma once

#include <charconv>
#include <concepts>
#include <cstring>
#include <string_view>
#include <system_error>
#include <type_traits>

#include "cct_config.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_string.hpp"
#include "mathhelpers.hpp"

namespace cct {

namespace details {
template <class SizeType>
void ToChars(char *first, SizeType sz, std::integral auto val) {
  if (auto [ptr, errc] = std::to_chars(first, first + sz, val); CCT_UNLIKELY(errc != std::errc())) {
    throw exception("Unable to decode integral into string");
  }
}
}  // namespace details

inline string ToString(std::integral auto val) {
  const int nbDigitsInt = nchars(val);
  string str(nbDigitsInt, '0');
  details::ToChars(str.data(), nbDigitsInt, val);
  return str;
}

inline auto ToCharVector(std::integral auto val) {
  // +1 for minus, +1 for additional partial ranges coverage
  using Int = decltype(val);
  static constexpr auto kMaxSize = std::numeric_limits<Int>::digits10 + 1 + (std::is_signed_v<Int> ? 1 : 0);

  FixedCapacityVector<char, kMaxSize> ret(nchars(val));

  auto [ptr, errc] = std::to_chars(ret.data(), ret.data() + ret.size(), val);
  if (errc != std::errc()) {
    throw exception("Unable to decode integral {} into string", val);
  }

  return ret;
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
void SetString(StringType &str, std::integral auto val) {
  const int nbDigitsInt = nchars(val);
  str.resize(nbDigitsInt, '0');
  details::ToChars(str.data(), nbDigitsInt, val);
}

template <class StringType>
void AppendString(StringType &str, std::integral auto val) {
  const int nbDigitsInt = nchars(val);
  str.append(nbDigitsInt, '0');
  details::ToChars(str.data() + static_cast<int>(str.size()) - nbDigitsInt, nbDigitsInt, val);
}

inline std::size_t strnlen(const char *start, std::size_t maxLen) {
  const void *outPtr = memchr(start, 0, maxLen);
  if (outPtr == nullptr) {
    return maxLen;
  }
  return static_cast<const char *>(outPtr) - start;
}

}  // namespace cct