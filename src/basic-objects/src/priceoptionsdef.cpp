#include "priceoptionsdef.hpp"

#include <string_view>

#include "enum-string.hpp"

namespace cct {

PriceStrategy StrategyFromStr(std::string_view priceStrategyStr) {
  return EnumFromString<PriceStrategy>(priceStrategyStr);
}

std::string_view PriceStrategyStr(PriceStrategy priceStrategy, bool placeRealOrderInSimulationMode) {
  if (placeRealOrderInSimulationMode) {
    return EnumToString(PriceStrategy::maker);
  }
  return EnumToString(priceStrategy);
}

}  // namespace cct