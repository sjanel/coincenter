#include "auto-trade-config.hpp"

#include <cstring>
#include <variant>

#include "durationstring.hpp"
#include "ndigits.hpp"
#include "stringconv.hpp"

namespace cct::schema {

namespace {
constexpr int kNbSignificantUnitsDuration = 10;
}

AutoTradeStopCriteriaValue::AutoTradeStopCriteriaValue(std::string_view valueStr) {
  if (valueStr.empty()) {
    throw invalid_argument("Unexpected str {} to parse AutoTradeStopCriteriaValue", valueStr);
  }
  if (valueStr.back() == '%') {
    valueStr.remove_suffix(1);
    _value = StringToIntegral(valueStr);
  } else {
    _value = ParseDuration(valueStr);
  }
}

char *AutoTradeStopCriteriaValue::appendTo(char *buf) const {
  return std::visit(
      [&buf](const auto &value) -> char * {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Duration>) {
          auto str = DurationToString(value, kNbSignificantUnitsDuration);
          std::memcpy(buf, str.data(), str.size());
          return buf + str.size();
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
          auto str = IntegralToCharVector(value);
          std::memcpy(buf, str.data(), str.size());
          return buf + str.size();
        } else {
          return buf;
        }
      },
      _value);
}

std::size_t AutoTradeStopCriteriaValue::strLen() const {
  return std::visit(
      [](const auto &value) -> std::size_t {
        if constexpr (std::is_same_v<std::decay_t<decltype(value)>, Duration>) {
          return DurationToString(value, kNbSignificantUnitsDuration).size();
        } else if constexpr (std::is_same_v<std::decay_t<decltype(value)>, int>) {
          return ndigits(value);
        } else {
          return 0;
        }
      },
      _value);
}

}  // namespace cct::schema