#include "traderesult.hpp"

namespace cct {
TradeResult::State TradeResult::state() const {
  // from could be lower than actually traded from amount if rounding issues
  if (_from <= _tradedAmounts.from) {
    return TradeResult::State::complete;
  }
  if (_tradedAmounts.from > 0) {
    return TradeResult::State::partial;
  }
  return TradeResult::State::untouched;
}
}  // namespace cct
