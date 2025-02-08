#pragma once

#include <unordered_map>

#include "currencycode.hpp"
#include "exchange-name-enum.hpp"
#include "monetaryamount.hpp"

namespace cct::schema {

using WithdrawFeesFilePerExchange = std::unordered_map<CurrencyCode, MonetaryAmount>;
using WithdrawFeesFile = std::unordered_map<ExchangeNameEnum, WithdrawFeesFilePerExchange>;

}  // namespace cct::schema