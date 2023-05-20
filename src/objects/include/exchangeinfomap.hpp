#pragma once

#include <map>
#include <string_view>

#include "cct_json.hpp"
#include "exchangeinfo.hpp"

namespace cct {

static constexpr std::string_view kPreferredPaymentCurrenciesOptName = "preferredPaymentCurrencies";

/// Map containing the ExchangeInfo per exchange name.
/// string_view is possible as key because exchange names are compile time std::string_view
using ExchangeInfoMap = std::map<std::string_view, ExchangeInfo, std::less<>>;

/// @brief Builds the exchange info map based on given json
ExchangeInfoMap ComputeExchangeInfoMap(std::string_view fileName, const json &jsonData);
}  // namespace cct