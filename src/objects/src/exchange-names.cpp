#include "exchange-names.hpp"

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

}  // namespace cct