#include "traderesult.hpp"

#include <string_view>

#include "cct_exception.hpp"

namespace cct {
TradeResult::State TradeResult::state() const {
  // from could be lower than actually traded from amount if rounding issues
  if (_from <= _tradedAmounts.from) {
    return TradeResult::State::kComplete;
  }
  if (_tradedAmounts.from > 0) {
    return TradeResult::State::kPartial;
  }
  return TradeResult::State::kUntouched;
}

std::string_view TradeResult::stateStr() const {
  switch (state()) {
    case State::kComplete:
      return "Complete";
    case State::kPartial:
      return "Partial";
    case State::kUntouched:
      return "Untouched";
    default:
      throw exception("Invalid state {}", static_cast<int>(state()));
  }
}
}  // namespace cct
