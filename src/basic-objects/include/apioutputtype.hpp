#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json.hpp"

namespace cct {

#define CCT_API_OUTPUT_TYPES off, table, json

enum class ApiOutputType : int8_t { CCT_API_OUTPUT_TYPES };

ApiOutputType ApiOutputTypeFromString(std::string_view str);

}  // namespace cct

template <>
struct glz::meta<::cct::ApiOutputType> {
  using enum ::cct::ApiOutputType;
  static constexpr auto value = enumerate(CCT_API_OUTPUT_TYPES);
};

#undef CCT_API_OUTPUT_TYPES
