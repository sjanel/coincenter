#pragma once

#include <algorithm>
#include <string_view>

#include "cct_string.hpp"
#include "toupperlower.hpp"

namespace cct {

inline string ToUpper(std::string_view str) {
  string ret(str);
  std::ranges::transform(ret, ret.begin(), [](char ch) { return toupper(ch); });
  return ret;
}

inline string ToLower(std::string_view str) {
  string ret(str);
  std::ranges::transform(ret, ret.begin(), [](char ch) { return tolower(ch); });
  return ret;
}

}  // namespace cct