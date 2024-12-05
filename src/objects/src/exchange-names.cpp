#include "exchange-names.hpp"

#include "cct_const.hpp"
#include "cct_string.hpp"

namespace cct {

string ConstructAccumulatedExchangeNames(ExchangeNameSpan exchangeNames) {
  // TODO: Use C++23 join_with feature
  string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const auto &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(exchangeName.str());
  }
  return exchangesStr;
}

string ConstructAccumulatedExchangeNames(ExchangeNameEnumSpan exchangeNames) {
  // TODO: Use C++23 join_with feature
  string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const auto &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(kSupportedExchanges[static_cast<int>(exchangeName)]);
  }
  return exchangesStr;
}

}  // namespace cct