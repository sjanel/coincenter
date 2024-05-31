#include "unitsparser.hpp"

#include <cstdint>
#include <string_view>

#include "cct_exception.hpp"
#include "stringconv.hpp"

namespace cct {

int64_t ParseNumberOfBytes(std::string_view sizeStr) {
  auto endPos = sizeStr.find_first_not_of("0123456789");
  if (endPos == std::string_view::npos) {
    endPos = sizeStr.size();
  }
  int64_t nbBytes = StringToIntegral<int64_t>(std::string_view(sizeStr.begin(), sizeStr.begin() + endPos));
  if (nbBytes < 0) {
    throw exception("Number of bytes cannot be negative");
  }
  int64_t multiplier = 1;
  if (endPos != sizeStr.size()) {
    bool iMultiplier = endPos + 1 < sizeStr.size() && sizeStr[endPos + 1] == 'i';
    int64_t multiplierBase = iMultiplier ? 1024L : 1000L;
    switch (sizeStr[endPos]) {
      case '.':
        throw exception("Decimal number not accepted for number of bytes parsing");
      case 'T':  // NOLINT(bugprone-branch-clone)
        multiplier *= multiplierBase;
        [[fallthrough]];
      case 'G':
        multiplier *= multiplierBase;
        [[fallthrough]];
      case 'M':
        multiplier *= multiplierBase;
        [[fallthrough]];
      case 'K':
        [[fallthrough]];
      case 'k':
        multiplier *= multiplierBase;
        break;
      default:
        throw exception("Invalid suffix '{}' for number of bytes parsing", sizeStr[endPos]);
    }
  }

  return nbBytes * multiplier;
}

}  // namespace cct