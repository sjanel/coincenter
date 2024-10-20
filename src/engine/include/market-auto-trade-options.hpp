#pragma once

#include <span>
#include <string_view>

#include "auto-trade-stop-criterion.hpp"
#include "cct_json-container.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"
#include "timedef.hpp"

namespace cct {

class MarketAutoTradeOptions {
 public:
  explicit MarketAutoTradeOptions(const json::container &data);

  std::span<const string> accounts() const { return _accounts; }

  std::string_view algorithmName() const { return _algorithmName; }

  Duration repeatTime() const { return _repeatTime; }

  MonetaryAmount baseStartAmount() const { return _baseStartAmount; }

  MonetaryAmount quoteStartAmount() const { return _quoteStartAmount; }

  std::span<const AutoTradeStopCriterion> stopCriterion() const { return _stopCriteria; }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  vector<string> _accounts;
  string _algorithmName;
  Duration _repeatTime;
  MonetaryAmount _baseStartAmount;
  MonetaryAmount _quoteStartAmount;
  vector<AutoTradeStopCriterion> _stopCriteria;
};

}  // namespace cct