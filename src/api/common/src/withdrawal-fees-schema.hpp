#pragma once

#include <cstdint>
#include <unordered_map>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct::schema {

struct WithdrawInfoFileItemAsset {
  MonetaryAmount min;  // only amount
  MonetaryAmount fee;  // only amount
};

struct WithdrawInfoFileItem {
  int64_t timeepoch;
  std::unordered_map<CurrencyCode, WithdrawInfoFileItemAsset> assets;
};

using WithdrawInfoFile = std::unordered_map<ExchangeNameEnum, WithdrawInfoFileItem>;

struct WithdrawFeesCrawlerExchangeFeesCoinSource1 {
  string symbol;

  auto operator<=>(const WithdrawFeesCrawlerExchangeFeesCoinSource1&) const = default;
};

struct WithdrawFeesCrawlerExchangeFeesSource1 {
  double amount;
  double min;
  WithdrawFeesCrawlerExchangeFeesCoinSource1 coin;

  auto operator<=>(const WithdrawFeesCrawlerExchangeFeesSource1&) const = default;
};

struct WithdrawFeesCrawlerExchangeSource1 {
  string name;
  vector<WithdrawFeesCrawlerExchangeFeesSource1> fees;
};

struct WithdrawFeesCrawlerSource1 {
  WithdrawFeesCrawlerExchangeSource1 exchange;
};

}  // namespace cct::schema