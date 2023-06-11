#pragma once

#include <cstdint>
#include <string_view>

namespace cct {
enum class ApiOutputType : int8_t {
  kNoPrint,
  kFormattedTable,
  kJson,
};

static constexpr std::string_view kApiOutputTypeNoPrintStr = "off";
static constexpr std::string_view kApiOutputTypeTableStr = "table";
static constexpr std::string_view kApiOutputTypeJsonStr = "json";

ApiOutputType ApiOutputTypeFromString(std::string_view str);
}  // namespace cct