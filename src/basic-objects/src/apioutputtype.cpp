#include "apioutputtype.hpp"

#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "toupperlower-string.hpp"

namespace cct {
ApiOutputType ApiOutputTypeFromString(std::string_view str) {
  auto lowerStr = ToLower(str);
  if (lowerStr == kApiOutputTypeNoPrintStr) {
    return ApiOutputType::off;
  }
  if (lowerStr == kApiOutputTypeTableStr) {
    return ApiOutputType::table;
  }
  if (lowerStr == kApiOutputTypeJsonStr) {
    return ApiOutputType::json;
  }
  throw invalid_argument("Unrecognized api output type {}", str);
}
}  // namespace cct