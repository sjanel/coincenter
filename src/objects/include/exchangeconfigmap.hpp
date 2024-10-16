#pragma once

#include <map>
#include <string_view>

#include "cct_json-container.hpp"
#include "exchangeconfig.hpp"

namespace cct {

static constexpr std::string_view kPreferredPaymentCurrenciesOptName = "preferredPaymentCurrencies";

/// Map containing the ExchangeConfig per exchange name.
/// string_view is possible as key because exchange names are compile time std::string_view constants
using ExchangeConfigMap = std::map<std::string_view, ExchangeConfig, std::less<>>;

/// @brief Builds the exchange info map based on given json
ExchangeConfigMap ComputeExchangeConfigMap(std::string_view fileName, const json::container &jsonData);
}  // namespace cct