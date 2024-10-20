#pragma once

#include <cstdint>
#include <string_view>
#include <variant>

#include "timedef.hpp"

namespace cct {

class AutoTradeStopCriterion {
 public:
  enum class Type : int8_t { kDuration, kProtectLoss, kSecureProfit };

  AutoTradeStopCriterion(std::string_view typeStr, std::string_view valueStr);

  Duration duration() const { return std::get<Duration>(_value); }

  int maxEvolutionPercentage() const { return std::get<int>(_value); }

  Type type() const { return _type; }

 private:
  using Value = std::variant<std::monostate, Duration, int>;

  Type _type;
  Value _value;
};

}  // namespace cct