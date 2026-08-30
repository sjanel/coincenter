#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "exchange-name-enum.hpp"
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

struct BithumbWithdrawalNetwork {
  std::optional<string> withdraw_fee_quantity;
  std::optional<string> withdraw_minimum_quantity;

  auto operator<=>(const BithumbWithdrawalNetwork&) const = default;
};

struct BithumbWithdrawalAsset {
  string currency;
  vector<BithumbWithdrawalNetwork> networks;

  auto operator<=>(const BithumbWithdrawalAsset&) const = default;
};

using BithumbWithdrawalFees = vector<BithumbWithdrawalAsset>;

struct KrakenWithdrawalNetwork {
  string network;

  auto operator<=>(const KrakenWithdrawalNetwork&) const = default;
};

struct KrakenWithdrawalMethod {
  string asset;
  string fee;
  string min_amount;
  KrakenWithdrawalNetwork withdrawal_network_info;

  auto operator<=>(const KrakenWithdrawalMethod&) const = default;
};

struct KrakenWithdrawalMethodsResponse {
  vector<KrakenWithdrawalMethod> result;

  auto operator<=>(const KrakenWithdrawalMethodsResponse&) const = default;
};

}  // namespace cct::schema
