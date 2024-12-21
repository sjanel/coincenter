#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json.hpp"

namespace cct {

enum class ApiOutputType : int8_t {
  off,
  table,
  json,
};

static constexpr std::string_view kApiOutputTypeNoPrintStr = "off";
static constexpr std::string_view kApiOutputTypeTableStr = "table";
static constexpr std::string_view kApiOutputTypeJsonStr = "json";

ApiOutputType ApiOutputTypeFromString(std::string_view str);

}  // namespace cct

template <>
struct glz::meta<::cct::ApiOutputType> {
  using enum ::cct::ApiOutputType;
  static constexpr auto value = enumerate(off, table, json);
};