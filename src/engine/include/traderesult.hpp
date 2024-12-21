#pragma once

#include <cstdint>

#include "cct_json.hpp"
#include "monetaryamount.hpp"
#include "tradedamounts.hpp"

namespace cct {
class TradeResult {
 public:
  enum class State : int8_t { complete, partial, untouched };

  TradeResult() noexcept = default;

  TradeResult(const TradedAmounts &tradedAmounts, MonetaryAmount from) : _tradedAmounts(tradedAmounts), _from(from) {}

  const TradedAmounts &tradedAmounts() const { return _tradedAmounts; }

  MonetaryAmount from() const { return _from; }

  bool isComplete() const { return state() == State::complete; }

  State state() const;

  constexpr bool operator==(const TradeResult &) const noexcept = default;

 private:
  TradedAmounts _tradedAmounts;
  MonetaryAmount _from;
};

}  // namespace cct

template <>
struct glz::meta<::cct::TradeResult::State> {
  using enum ::cct::TradeResult::State;

  static constexpr auto value = enumerate(complete, partial, untouched);
};