#include "apioutputtype.hpp"

#include <string_view>

#include "enum-string.hpp"

namespace cct {

ApiOutputType ApiOutputTypeFromString(std::string_view str) {
  return EnumFromStringCaseInsensitive<ApiOutputType>(str);
}

}  // namespace cct