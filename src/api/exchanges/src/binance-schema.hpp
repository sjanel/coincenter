#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::binance {

using OrderId = uint64_t;

// https://binance-docs.github.io/apidocs/spot/en/#account-status-user_data
struct V1AccountStatus {
  string data;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#account-information-user_data
struct V3AccountBalance {
  struct Asset {
    string asset;
    MonetaryAmount free;    // without unit
    MonetaryAmount locked;  // without unit

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Asset&) const = default;
  };

  vector<Asset> balances;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#fetch-deposit-address-list-with-network-user_data
struct V1CapitalDepositAddressListElement {
  string address;
  string tag;

  std::optional<int> code;
  std::optional<string> msg;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1CapitalDepositAddressListElement&) const = default;
};

// https://binance-docs.github.io/apidocs/spot/en/#all-orders-user_data
// https://binance-docs.github.io/apidocs/spot/en/#cancel-all-open-orders-on-a-symbol-trade
struct V3GetAllOrder {
  string symbol;
  int64_t time{};
  OrderId orderId{};
  MonetaryAmount executedQty;
  MonetaryAmount price;
  string side;
  MonetaryAmount origQty;
  int64_t updateTime{};

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V3GetAllOrder&) const = default;
};

using V3GetAllOrders = vector<V3GetAllOrder>;

// https://binance-docs.github.io/apidocs/spot/en/#cancel-all-open-orders-on-a-symbol-trade
struct V3CancelOrder {
  OrderId orderId{};
};

using V3CancelAllOrders = vector<V3CancelOrder>;

// https://binance-docs.github.io/apidocs/spot/en/#deposit-history-supporting-network-user_data
struct V1CapitalDeposit {
  int64_t status{-1};
  string coin;
  string id;
  string address;
  double amount{};
  int64_t insertTime{};

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using V1CapitalDepositHisRec = vector<V1CapitalDeposit>;

// https://binance-docs.github.io/apidocs/spot/en/#withdraw-history-supporting-network-user_data
struct V1CapitalWithdraw {
  int64_t status{-1};
  string coin;
  string id;
  double amount{};
  double transactionFee{};
  int64_t applyTime{};
  int64_t completeTime{};

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using V1CapitalWithdrawHistory = vector<V1CapitalWithdraw>;

// https://binance-docs.github.io/apidocs/spot/en/#asset-detail-user_data
struct V1AssetDetail {
  MonetaryAmount withdrawFee;
  bool withdrawStatus{};
};

using V1AssetDetailMap = std::unordered_map<string, V1AssetDetail>;

// https://binance-docs.github.io/apidocs/spot/en/#dust-transfer-user_data

struct V1AssetDustResult {
  OrderId tranId{};
  MonetaryAmount transferedAmount;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1AssetDustResult&) const = default;
};

struct V1AssetDust {
  SmallVector<V1AssetDustResult, 1> transferResult;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#new-order-trade

struct V3NewOrderFills {
  MonetaryAmount price;
  MonetaryAmount qty;
  MonetaryAmount commission;
  CurrencyCode commissionAsset;
  OrderId orderId{};

  auto operator<=>(const V3NewOrderFills&) const = default;
};

struct V3NewOrder {
  string status;
  OrderId orderId{};
  SmallVector<V3NewOrderFills, 1> fills;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

// https://binance-docs.github.io/apidocs/spot/en/#query-order-user_data

struct V3GetOrder {
  string status;
  int64_t time{};

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

// https://binance-docs.github.io/apidocs/spot/en/#account-trade-list-user_data

using V3MyTrades = vector<V3NewOrderFills>;

// https://binance-docs.github.io/apidocs/spot/en/#withdraw-user_data

struct V1CapitalWithdrawApply {
  string id;
};

}  // namespace cct::schema::binance