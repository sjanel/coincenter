#include "auto-trade-stop-criterion.hpp"

#include "cct_invalid_argument_exception.hpp"
#include "durationstring.hpp"
#include "stringconv.hpp"

namespace cct {

namespace {
auto TypeFromStr(std::string_view typeStr) {
  if (typeStr == "duration") {
    return AutoTradeStopCriterion::Type::kDuration;
  }
  if (typeStr == "protectLoss") {
    return AutoTradeStopCriterion::Type::kProtectLoss;
  }
  if (typeStr == "secureProfit") {
    return AutoTradeStopCriterion::Type::kSecureProfit;
  }
  throw invalid_argument("Unknown stop criterion type {}", typeStr);
}

auto PercentageIntFromStr(std::string_view valueStr) {
  const std::string_view integralStr = valueStr.substr(0, valueStr.find('%'));
  return StringToIntegral(integralStr);
}

}  // namespace

AutoTradeStopCriterion::AutoTradeStopCriterion(std::string_view typeStr, std::string_view valueStr)
    : _type(TypeFromStr(typeStr)), _value() {
  switch (_type) {
    case Type::kDuration:
      _value = ParseDuration(valueStr);
      break;
    case Type::kProtectLoss:
      [[fallthrough]];
    case Type::kSecureProfit:
      _value = PercentageIntFromStr(valueStr);
      break;
    default: {
      throw invalid_argument("Unknown stop criterion type {}", static_cast<int>(_type));
    }
  }
}

}  // namespace cct