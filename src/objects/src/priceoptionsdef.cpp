#include "priceoptionsdef.hpp"

#include <string_view>

#include "cct_invalid_argument_exception.hpp"
#include "unreachable.hpp"

namespace cct {
namespace {
constexpr std::string_view kMakerStr = "maker";
constexpr std::string_view kNibbleStr = "nibble";
constexpr std::string_view kTakerStr = "taker";
}  // namespace

PriceStrategy StrategyFromStr(std::string_view priceStrategyStr) {
  if (priceStrategyStr == kMakerStr) {
    return PriceStrategy::kMaker;
  }
  if (priceStrategyStr == kNibbleStr) {
    return PriceStrategy::kNibble;
  }
  if (priceStrategyStr == kTakerStr) {
    return PriceStrategy::kTaker;
  }

  throw invalid_argument("Unrecognized price strategy, possible values are '{}', '{}' and '{}'", kMakerStr, kNibbleStr,
                         kTakerStr);
}

std::string_view PriceStrategyStr(PriceStrategy priceStrategy, bool placeRealOrderInSimulationMode) {
  if (placeRealOrderInSimulationMode) {
    return kMakerStr;
  }
  switch (priceStrategy) {
    case PriceStrategy::kMaker:
      return kMakerStr;
    case PriceStrategy::kNibble:
      return kNibbleStr;
    case PriceStrategy::kTaker:
      return kTakerStr;
    default:
      unreachable();
  }
}
}  // namespace cct