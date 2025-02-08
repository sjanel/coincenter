#include "apioutputtype.hpp"

#include <algorithm>
#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "cct_json.hpp"
#include "string-equal-ignore-case.hpp"

namespace cct {
ApiOutputType ApiOutputTypeFromString(std::string_view str) {
  static constexpr auto kOutputTypes = json::reflect<ApiOutputType>::keys;

  auto pos =
      std::ranges::find_if(kOutputTypes,
                           [str](std::string_view apiOutputType) { return CaseInsensitiveEqual(apiOutputType, str); }) -
      std::begin(kOutputTypes);
  if (pos == std::size(kOutputTypes)) {
    throw invalid_argument("Unrecognized api output type {}", str);
  }
  return static_cast<ApiOutputType>(pos);
}
}  // namespace cct