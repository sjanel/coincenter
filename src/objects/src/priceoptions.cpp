#include "priceoptions.hpp"

#include <stdexcept>

#include "cct_invalid_argument_exception.hpp"
#include "stringhelpers.hpp"
#include "unreachable.hpp"

namespace cct {
namespace {

constexpr std::string_view kMakerStr = "maker";
constexpr std::string_view kNibbleStr = "nibble";
constexpr std::string_view kTakerStr = "taker";

constexpr PriceStrategy StrategyFromStr(std::string_view priceStrategyStr) {
  if (priceStrategyStr == kMakerStr) {
    return PriceStrategy::kMaker;
  }
  if (priceStrategyStr == kNibbleStr) {
    return PriceStrategy::kNibble;
  }
  if (priceStrategyStr == kTakerStr) {
    return PriceStrategy::kTaker;
  }

  throw invalid_argument("Unrecognized price strategy");
}
}  // namespace

PriceOptions::PriceOptions(std::string_view priceStrategyStr) : _priceStrategy(StrategyFromStr(priceStrategyStr)) {}

PriceOptions::PriceOptions(RelativePrice relativePrice) : _relativePrice(relativePrice) {
  if (relativePrice == 0 || relativePrice == kNoRelativePrice) {
    throw invalid_argument("Invalid relative price");
  }
}

std::string_view PriceOptions::priceStrategyStr(bool placeRealOrderInSimulationMode) const {
  if (placeRealOrderInSimulationMode) {
    return kMakerStr;
  }
  switch (_priceStrategy) {
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

string PriceOptions::str(bool placeRealOrderInSimulationMode) const {
  string ret;
  ret.append(priceStrategyStr(placeRealOrderInSimulationMode));
  ret.append(" strategy");
  return ret;
}
}  // namespace cct