#include "exchange-names.hpp"

namespace cct {

string ConstructAccumulatedExchangeNames(ExchangeNameSpan exchangeNames) {
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