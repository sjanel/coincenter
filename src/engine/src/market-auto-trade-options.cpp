#include "market-auto-trade-options.hpp"

#include "cct_invalid_argument_exception.hpp"
#include "durationstring.hpp"

namespace cct {

namespace {
auto GetFieldOrThrow(const json &data, std::string_view fieldName) {
  const auto it = data.find(fieldName);
  if (it == data.end()) {
    throw invalid_argument("Expected field '{}' in auto trade configuration {}", fieldName, data.dump());
  }
  return it;
}
}  // namespace

MarketAutoTradeOptions::MarketAutoTradeOptions(const json &data)
    : _accounts(),
      _algorithmName(GetFieldOrThrow(data, "algorithmName")->get<std::string_view>()),
      _repeatTime(ParseDuration(GetFieldOrThrow(data, "repeatTime")->get<std::string_view>())),
      _baseStartAmount(GetFieldOrThrow(data, "baseStartAmount")->get<std::string_view>()),
      _quoteStartAmount(GetFieldOrThrow(data, "quoteStartAmount")->get<std::string_view>()),
      _stopCriteria() {
  const auto accountsIt = GetFieldOrThrow(data, "accounts");
  if (!accountsIt->is_array()) {
    throw invalid_argument("Expected 'accounts' field to be an array in auto trade configuration {}", data.dump());
  }
  if (accountsIt->empty()) {
    throw invalid_argument("Expected 'accounts' field to be non empty in auto trade configuration {}", data.dump());
  }
  _accounts.reserve(accountsIt->size());
  std::ranges::transform(*accountsIt, std::back_inserter(_accounts),
                         [](const json &accountJson) { return string(accountJson.get<std::string_view>()); });

  const auto stopCritJsonIt = GetFieldOrThrow(data, "stopCriteria");
  if (!stopCritJsonIt->is_array()) {
    throw invalid_argument("Expected 'stopCriteria' field to be an array in auto trade configuration {}", data.dump());
  }
  _stopCriteria.reserve(stopCritJsonIt->size());
  std::ranges::transform(*stopCritJsonIt, std::back_inserter(_stopCriteria), [](const json &stopCrit) {
    return AutoTradeStopCriterion(GetFieldOrThrow(stopCrit, "type")->get<std::string_view>(),
                                  GetFieldOrThrow(stopCrit, "value")->get<std::string_view>());
  });
}

}  // namespace cct