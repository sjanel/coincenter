#pragma once

#include <string_view>

#include "toupperlower.hpp"

namespace cct {

constexpr bool CaseInsensitiveEqual(std::string_view lhs, std::string_view rhs) {
  const auto lhsSize = lhs.size();
  if (lhsSize != rhs.size()) {
    return false;
  }
  for (std::string_view::size_type charPos{}; charPos < lhsSize; ++charPos) {
    if (toupper(lhs[charPos]) != toupper(rhs[charPos])) {
      return false;
    }
  }
  return true;
}

}  // namespace cct