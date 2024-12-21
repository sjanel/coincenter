#include "coincentercommandtype.hpp"

#include <string_view>
#include <type_traits>

namespace cct {

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
