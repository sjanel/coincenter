#include "coincentercommandtype.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "cct_exception.hpp"
#include "cct_json-serialization.hpp"

namespace cct {
namespace {
constexpr auto kCommandTypeNames = json::reflect<CoincenterCommandType>::keys;

static_assert(std::size(kCommandTypeNames) == static_cast<std::size_t>(CoincenterCommandType::Last) + 1);

}  // namespace

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type) {
  const auto intValue = static_cast<std::underlying_type_t<CoincenterCommandType>>(type);
  if (intValue < decltype(intValue){} ||
      intValue >= static_cast<std::underlying_type_t<CoincenterCommandType>>(CoincenterCommandType::Last)) {
    throw exception("Unknown command type {}", intValue);
  }
  return kCommandTypeNames[intValue];
}

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str) {
  const auto cmdIt = std::ranges::find(kCommandTypeNames, str);
  if (cmdIt == std::end(kCommandTypeNames)) {
    throw exception("Unknown command type {}", str);
  }
  return static_cast<CoincenterCommandType>(cmdIt - std::begin(kCommandTypeNames));
}

bool IsAnyTrade(CoincenterCommandType type) {
  switch (type) {
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
