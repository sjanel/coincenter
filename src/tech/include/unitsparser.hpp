#pragma once

#include <cstdint>
#include <string_view>

#include "cct_exception.hpp"
#include "stringhelpers.hpp"

namespace cct {
/// Parses a string representation of a number of bytes.
/// string should contain an integral number (decimal not supported) possibly followed by one of these units:
///  - T, G, M, k for multiples of 1000
///  - Ti, Gi, Mi, Ki for multiples of 1024
/// Note: it is a simplified version of the syntax used by Kubernetes:
/// https://kubernetes.io/docs/concepts/configuration/manage-resources-containers/
inline int64_t ParseNumberOfBytes(std::string_view sizeStr) {
  auto endPos = sizeStr.find_first_not_of("0123456789");
  int64_t v = FromString<int64_t>(std::string_view(sizeStr.begin(), sizeStr.begin() + endPos));
  int64_t multiplier = 1;
  endPos = sizeStr.find_first_of("TGMkK.", endPos);
  if (endPos != std::string_view::npos) {
    bool iMultiplier = endPos + 1 < sizeStr.size() && sizeStr[endPos + 1] == 'i';
    switch (sizeStr[endPos]) {
      case '.':
        throw exception("Decimal number not accepted for number of bytes parsing");
      case 'T':
        multiplier = iMultiplier ? 1099511627776L : 1000000000000L;
        break;
      case 'G':
        multiplier = iMultiplier ? 1073741824L : 1000000000L;
        break;
      case 'M':
        multiplier = iMultiplier ? 1048576L : 1000000L;
        break;
      case 'K':
        [[fallthrough]];
      case 'k':
        multiplier = iMultiplier ? 1024L : 1000L;
        break;
      default:
        break;
    }
  }

  return v * multiplier;
}
}  // namespace