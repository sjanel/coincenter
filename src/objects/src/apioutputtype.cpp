#include "apioutputtype.hpp"

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "toupperlower.hpp"

namespace cct {
ApiOutputType ApiOutputTypeFromString(std::string_view str) {
  string lowerStr = ToLower(str);
  if (lowerStr == kApiOutputTypeNoPrintStr) {
    return ApiOutputType::kNoPrint;
  }
  if (lowerStr == kApiOutputTypeTableStr) {
    return ApiOutputType::kFormattedTable;
  }
  if (lowerStr == kApiOutputTypeJsonStr) {
    return ApiOutputType::kJson;
  }
  string err("Unrecognized api output type ");
  err.append(str);
  throw invalid_argument(std::move(err));
}
}  // namespace cct