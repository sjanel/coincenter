#pragma once

#include <span>
#include <string_view>

#include "auto-trade-stop-criterion.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {

class MarketAutoTradeOptions {
 public:
  explicit MarketAutoTradeOptions(const json &data);

  std::string_view algorithmName() const { return _algorithmName; }

  Duration repeatTime() const { return _repeatTime; }

  std::span<const AutoTradeStopCriterion> stopCriterion() const { return _stopCriteria; }

 private:
  string _algorithmName;
  Duration _repeatTime;
  MonetaryAmount _baseStartAmount;
  MonetaryAmount _quoteStartAmount;
  vector<AutoTradeStopCriterion> _stopCriteria;
};

}  // namespace cct