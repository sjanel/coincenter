#pragma once

#include <map>
#include <string_view>

#include "cct_string.hpp"
#include "currencycode.hpp"

namespace cct {

using AccountDepositAddresses = std::map<CurrencyCode, string, std::less<>>;

using ExchangeDepositAddresses = std::map<string, AccountDepositAddresses, std::less<>>;

using DepositAddresses = std::map<string, ExchangeDepositAddresses, std::less<>>;

DepositAddresses ReadDepositAddresses(std::string_view dataDir);

}  // namespace cct
