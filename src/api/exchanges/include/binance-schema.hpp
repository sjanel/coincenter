#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::binance {

template <class T>
using has_code_t = decltype(std::declval<T>().code);

template <class T>
using has_msg_t = decltype(std::declval<T>().msg);

// PUBLIC

// https://binance-docs.github.io/apidocs/spot/en/#exchange-information

struct V3ExchangeInfo {
  struct Symbol {
    string baseAsset;
    string quoteAsset;
    string status;
    int8_t baseAssetPrecision;
    int8_t quoteAssetPrecision;

    struct Filter {
      string filterType;
      MonetaryAmount maxPrice;
      MonetaryAmount minPrice;
      MonetaryAmount tickSize;
      MonetaryAmount minNotional;
      MonetaryAmount maxNotional;
      MonetaryAmount maxQty;
      MonetaryAmount minQty;
      MonetaryAmount stepSize;
      int32_t avgPriceMins;
      bool applyToMarket;
      bool applyMinToMarket;
      bool applyMaxToMarket;

      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Filter&) const = default;
    };

    vector<Filter> filters;

    vector<string> permissions;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Symbol&) const = default;
  };

  vector<Symbol> symbols;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#current-average-price
struct V3AvgPrice {
  MonetaryAmount price;
  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#symbol-order-book-ticker
struct V3TickerBookTickerElem {
  string symbol;
  MonetaryAmount bidPrice;
  MonetaryAmount bidQty;
  MonetaryAmount askPrice;
  MonetaryAmount askQty;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using V3TickerBookTicker = vector<V3TickerBookTickerElem>;

// https://binance-docs.github.io/apidocs/spot/en/#order-book
struct V3OrderBook {
  using Line = std::array<MonetaryAmount, 2U>;  // price is first, then volume

  vector<Line> asks;
  vector<Line> bids;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#24hr-ticker-price-change-statistics
struct V3Ticker24hr {
  MonetaryAmount volume;

  std::optional<int> code;
  std::optional<string> msg;
};

// https://binance-docs.github.io/apidocs/spot/en/#recent-trades-list
struct V3Trade {
  MonetaryAmount price;
  MonetaryAmount qty;
  int64_t time;
  bool isBuyerMaker;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V3Trade&) const = default;
};

using V3Trades = vector<V3Trade>;

// https://binance-docs.github.io/apidocs/spot/en/#symbol-price-ticker
struct V3TickerPrice {
  MonetaryAmount price;

  std::optional<int> code;
  std::optional<string> msg;
};

// PRIVATE

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
  int64_t time;
  OrderId orderId;
  MonetaryAmount executedQty;
  MonetaryAmount price;
  string side;
  MonetaryAmount origQty;
  int64_t updateTime;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V3GetAllOrder&) const = default;
};

using V3GetAllOrders = vector<V3GetAllOrder>;

// https://binance-docs.github.io/apidocs/spot/en/#cancel-all-open-orders-on-a-symbol-trade
struct V3CancelOrder {
  OrderId orderId;
};

using V3CancelAllOrders = vector<V3CancelOrder>;

// https://binance-docs.github.io/apidocs/spot/en/#deposit-history-supporting-network-user_data
struct V1CapitalDeposit {
  int64_t status = -1;
  string coin;
  string id;
  string address;
  double amount;
  int64_t insertTime;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using V1CapitalDepositHisRec = vector<V1CapitalDeposit>;

// https://binance-docs.github.io/apidocs/spot/en/#withdraw-history-supporting-network-user_data
struct V1CapitalWithdraw {
  int64_t status = -1;
  string coin;
  string id;
  double amount;
  double transactionFee;
  int64_t applyTime;
  int64_t completeTime;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

using V1CapitalWithdrawHistory = vector<V1CapitalWithdraw>;

// https://binance-docs.github.io/apidocs/spot/en/#asset-detail-user_data
struct V1AssetDetail {
  MonetaryAmount withdrawFee;
  bool withdrawStatus;
};

using V1AssetDetailMap = std::unordered_map<string, V1AssetDetail>;

// https://binance-docs.github.io/apidocs/spot/en/#dust-transfer-user_data

struct V1AssetDustResult {
  OrderId tranId;
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
  OrderId orderId;

  auto operator<=>(const V3NewOrderFills&) const = default;
};

struct V3NewOrder {
  string status;
  OrderId orderId = -1;
  SmallVector<V3NewOrderFills, 1> fills;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

// https://binance-docs.github.io/apidocs/spot/en/#query-order-user_data

struct V3GetOrder {
  string status;
  int64_t time;

  using trivially_relocatable = is_trivially_relocatable<string>::type;
};

// https://binance-docs.github.io/apidocs/spot/en/#account-trade-list-user_data

using V3MyTrades = vector<V3NewOrderFills>;

// https://binance-docs.github.io/apidocs/spot/en/#withdraw-user_data

struct V1CapitalWithdrawApply {
  string id;
};

}  // namespace cct::schema::binance