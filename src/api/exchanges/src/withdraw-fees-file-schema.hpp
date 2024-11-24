#pragma once

#include <unordered_map>

#include "cct_const.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct::schema {

using WithdrawFeesFilePerExchange = std::unordered_map<CurrencyCode, MonetaryAmount>;
using WithdrawFeesFile = std::unordered_map<ExchangeNameEnum, WithdrawFeesFilePerExchange>;

}  // namespace cct::schema