#pragma once

#include <cstdint>
#include <string_view>

#include "monetaryamount.hpp"
#include "tradedamounts.hpp"

namespace cct {
class TradeResult {
 public:
  enum class State : int8_t { kComplete, kPartial, kUntouched };

  TradeResult() noexcept = default;

  TradeResult(const TradedAmounts &tradedAmounts, MonetaryAmount from) : _tradedAmounts(tradedAmounts), _from(from) {}

  const TradedAmounts &tradedAmounts() const { return _tradedAmounts; }

  MonetaryAmount from() const { return _from; }

  bool isComplete() const { return state() == State::kComplete; }

  State state() const;

  std::string_view stateStr() const;

  constexpr bool operator==(const TradeResult &) const noexcept = default;

 private:
  TradedAmounts _tradedAmounts;
  MonetaryAmount _from;
};

}  // namespace cct
