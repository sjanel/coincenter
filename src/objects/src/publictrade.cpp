#include "publictrade.hpp"

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "timestring.hpp"

namespace cct {

string PublicTrade::timeStr() const { return ToString(_time); }

bool PublicTrade::isValid() const {
  if (time() == TimePoint{}) {
    log::error("Public trade is invalid as no timestamp");
    return false;
  }
  if (amount() <= 0 || amount().hasNeutralCurrency()) {
    log::error("Public trade has an invalid amount");
    return false;
  }
  if (price() <= 0 || price().hasNeutralCurrency()) {
    log::error("Public trade has an invalid price");
    return false;
  }
  if (amount().currencyCode() == price().currencyCode()) {
    log::error("Public trade has an invalid market");
    return false;
  }
  if (side() != TradeSide::kBuy && side() != TradeSide::kSell) {
    log::error("Public trade has trade side");
    return false;
  }
  return true;
}

}  // namespace cct