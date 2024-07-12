#pragma once

#include <variant>

#include "timedef.hpp"

namespace cct {

class AutoTradeStopCriterion {
 public:
  enum class Type : int8_t { kDuration, kProtectLoss, kSecureProfit };

  explicit AutoTradeStopCriterion(Duration duration) : _value(duration), _type(Type::kDuration) {}

  explicit AutoTradeStopCriterion(int maxEvolutionPercentage)
      : _value(maxEvolutionPercentage), _type(maxEvolutionPercentage < 0 ? Type::kProtectLoss : Type::kSecureProfit) {}

  Duration duration() const { return std::get<Duration>(_value); }

  int maxEvolutionPercentage() const { return std::get<int>(_value); }

  Type type() const { return _type; }

 private:
  using Value = std::variant<Duration, int>;

  Value _value;
  Type _type;
};

static_assert(std::is_trivially_copyable_v<AutoTradeStopCriterion>);

}  // namespace cct