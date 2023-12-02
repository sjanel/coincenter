#include "apioutputtype.hpp"

#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "toupperlower-string.hpp"

namespace cct {
ApiOutputType ApiOutputTypeFromString(std::string_view str) {
  auto lowerStr = ToLower(str);
  if (lowerStr == kApiOutputTypeNoPrintStr) {
    return ApiOutputType::kNoPrint;
  }
  if (lowerStr == kApiOutputTypeTableStr) {
    return ApiOutputType::kFormattedTable;
  }
  if (lowerStr == kApiOutputTypeJsonStr) {
    return ApiOutputType::kJson;
  }
  throw invalid_argument("Unrecognized api output type {}", str);
}
}  // namespace cct