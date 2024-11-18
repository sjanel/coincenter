#include "coincentercommandtype.hpp"

#include <string_view>
#include <type_traits>

#include "cct_json-serialization.hpp"

namespace cct {
namespace {
constexpr auto kCommandTypeNames = json::reflect<CoincenterCommandType>::keys;
}  // namespace

std::string_view CoincenterCommandTypeToString(CoincenterCommandType coincenterCommandType) {
  return kCommandTypeNames[static_cast<std::underlying_type_t<CoincenterCommandType>>(coincenterCommandType)];
}

bool IsAnyTrade(CoincenterCommandType coincenterCommandType) {
  switch (coincenterCommandType) {
    case CoincenterCommandType::Trade:
      [[fallthrough]];
    case CoincenterCommandType::Buy:
      [[fallthrough]];
    case CoincenterCommandType::Sell:
      return true;
    default:
      return false;
  }
}
}  // namespace cct
