#include "publictrade.hpp"

#include "cct_string.hpp"
#include "timestring.hpp"

namespace cct {

string PublicTrade::timeStr() const { return ToString(_time); }

bool PublicTrade::isValid() const {
  if (time() == TimePoint{}) {
    return false;
  }
  if (amount() <= 0 || amount().hasNeutralCurrency()) {
    return false;
  }
  if (price() <= 0 || price().hasNeutralCurrency()) {
    return false;
  }
  if (amount().currencyCode() == price().currencyCode()) {
    return false;
  }
  if (side() != TradeSide::kBuy && side() != TradeSide::kSell) {
    return false;
  }
  return true;
}

}  // namespace cct