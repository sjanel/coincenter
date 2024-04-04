#include "publictrade.hpp"

#include "cct_log.hpp"
#include "cct_string.hpp"
#include "timedef.hpp"
#include "timestring.hpp"
#include "tradeside.hpp"

namespace cct {

string PublicTrade::timeStr() const { return ToString(_time); }

bool PublicTrade::isValid() const {
  if (time() == TimePoint{}) {
    log::error("Public trade is invalid as it has no timestamp");
    return false;
  }
  if (amount() <= 0 || amount().hasNeutralCurrency()) {
    log::error("Public trade has an invalid amount {}", amount());
    return false;
  }
  if (price() <= 0 || price().hasNeutralCurrency()) {
    log::error("Public trade has an invalid price {}", price());
    return false;
  }
  if (amount().currencyCode() == price().currencyCode()) {
    log::error("Public trade has an invalid market {}", market());
    return false;
  }
  if (side() != TradeSide::kBuy && side() != TradeSide::kSell) {
    log::error("Public trade has an invalid trade side {}", static_cast<int>(side()));
    return false;
  }
  return true;
}

}  // namespace cct