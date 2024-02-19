#include "priceoptions.hpp"

#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "cct_string.hpp"
#include "monetaryamount.hpp"
#include "priceoptionsdef.hpp"
#include "tradeconfig.hpp"

namespace cct {

PriceOptions::PriceOptions(std::string_view priceStrategyStr)
    : _priceStrategy(StrategyFromStr(priceStrategyStr)), _isDefault(false) {}

PriceOptions::PriceOptions(RelativePrice relativePrice) : _relativePrice(relativePrice), _isDefault(false) {
  if (relativePrice == 0 || relativePrice == kNoRelativePrice) {
    throw invalid_argument("Invalid relative price, should be non zero");
  }
}

PriceOptions::PriceOptions(const TradeConfig &tradeConfig)
    : _priceStrategy(tradeConfig.tradeStrategy()), _isDefault(false) {}

PriceOptions::PriceOptions(MonetaryAmount fixedPrice) : _fixedPrice(fixedPrice), _isDefault(false) {}

string PriceOptions::str(bool placeRealOrderInSimulationMode) const {
  string ret;
  ret.append(PriceStrategyStr(_priceStrategy, placeRealOrderInSimulationMode));
  ret.append(" strategy");
  return ret;
}
}  // namespace cct