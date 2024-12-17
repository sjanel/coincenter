#pragma once

#include <cstdint>
#include <glaze/glaze.hpp>  // IWYU pragma: export

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::huobi {

template <class T>
using has_code_t = decltype(std::declval<T>().code);

template <class T>
using has_status_t = decltype(std::declval<T>().status);

// PUBLIC

// https://huobiapi.github.io/docs/spot/v1/en/#get-system-status

struct V2SystemStatus {
  struct Status {
    string description;
  };

  struct Incidents {
    auto operator<=>(const Incidents&) const = default;
  };

  vector<Incidents> incidents;

  Status status;
};

// https://huobiapi.github.io/docs/spot/v1/en/#apiv2-currency-amp-chains

struct V2ReferenceCurrencyDetails {
  string currency;
  string instStatus;

  struct Chain {
    string chain;
    string displayName;
    string depositStatus;
    string withdrawStatus;
    string withdrawFeeType;
    string transactFeeWithdraw;

    MonetaryAmount minWithdrawAmt;
    MonetaryAmount maxWithdrawAmt;
    int8_t withdrawPrecision;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Chain&) const = default;
  };

  vector<Chain> chains;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V2ReferenceCurrencyDetails&) const = default;
};

struct V2ReferenceCurrency {
  int code;
  vector<V2ReferenceCurrencyDetails> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-all-supported-trading-symbol-v2

struct V1SettingsCommonMarketSymbol {
  string bc;  // base currency
  string qc;  // quote currency
  string state;
  string at;
  int8_t ap;
  int8_t pp;
  double minov;
  double maxov;
  double lominoa;
  double lomaxoa;
  double smminoa;
  double smmaxoa;
  double bmmaxov;

  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1SettingsCommonMarketSymbol&) const = default;
};

struct V1SettingsCommonMarketSymbols {
  string status;
  vector<V1SettingsCommonMarketSymbol> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-latest-tickers-for-all-pairs

struct MarketTickers {
  string status;

  struct Ticker {
    string symbol;
    double ask;
    double bid;
    double askSize;
    double bidSize;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Ticker&) const = default;
  };

  vector<Ticker> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-market-depth

struct MarketDepth {
  string status;

  struct Tick {
    using PriceQuantityPair = std::array<double, 2>;

    vector<PriceQuantityPair> asks;
    vector<PriceQuantityPair> bids;
  };

  Tick tick;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-latest-aggregated-ticker

struct MarketDetailMerged {
  string status;

  struct Tick {
    double amount;
  };

  Tick tick;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-the-most-recent-trades

struct MarketHistoryTrade {
  string status;

  struct Trade {
    struct TradeData {
      double amount;
      double price;
      int64_t ts;

      enum class Direction : int8_t { buy, sell };

      Direction direction;

      auto operator<=>(const TradeData&) const = default;
    };

    vector<TradeData> data;

    auto operator<=>(const Trade&) const = default;
  };

  vector<Trade> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-the-last-trade

struct MarketTrade {
  string status;
  struct Tick {
    struct Data {
      double price;

      auto operator<=>(const Data&) const = default;
    };
    vector<Data> data;
  };
  Tick tick;
};

// PRIVATE

// https://huobiapi.github.io/docs/spot/v1/en/#get-all-accounts-of-the-current-user

struct V1AccountAccounts {
  string status;

  struct Item {
    int64_t id;
    string state;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-account-balance-of-a-specific-account

struct V1AccountAccountsBalance {
  string status;

  struct Data {
    struct Item {
      string type;
      string currency;
      MonetaryAmount balance;

      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Item&) const = default;
    };

    vector<Item> list;
  };

  Data data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#query-deposit-address

struct V2AccountDepositAddress {
  int code;

  struct Item {
    string address;
    string addressTag;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#search-past-orders

// https://huobiapi.github.io/docs/spot/v1/en/#search-historical-orders-within-48-hours

struct V1Orders {
  string status;

  struct Item {
    string symbol;
    MonetaryAmount fieldAmount;  // field-amount
    MonetaryAmount price;
    string type;
    int64_t createdAt;  // created-at
    int64_t id;
    int64_t finishedAt;  // finished-at

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-all-open-orders

struct V1OrderOpenOrders {
  string status;

  struct Item {
    string symbol;
    MonetaryAmount amount;
    MonetaryAmount price;
    MonetaryAmount filledAmount;  // filled-amount
    int64_t createdAt;            // created-at
    int64_t id;
    string type;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#search-for-existed-withdraws-and-deposits

struct V1QueryDepositWithdraw {
  string status;

  struct Item {
    string state;
    string currency;
    int64_t id;
    double amount;
    double fee;
    int64_t updatedAt;  // updated-at

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#submit-cancel-for-multiple-orders-by-ids

struct V1OrderOrdersBatchCancel {
  string status;
};

// https://huobiapi.github.io/docs/spot/v1/en/#place-a-new-order

struct V1OrderOrdersPlace {
  string status;
  string data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#submit-cancel-for-an-order

struct V1OrderOrdersSubmitCancel {
  string status;
};

// https://huobiapi.github.io/docs/spot/v1/en/#get-the-order-detail-of-an-order

struct V1OrderOrdersDetail {
  string status;

  struct Data {
    string state;

    MonetaryAmount fieldAmount;      // field-amount
    MonetaryAmount fieldCashAmount;  // field-cash-amount
    MonetaryAmount fieldFees;        // field-fees

    MonetaryAmount filledAmount;      // filled-amount
    MonetaryAmount filledCashAmount;  // filled-cash-amount
    MonetaryAmount filledFees;        // filled-fees

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Data&) const = default;
  };

  Data data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#query-withdraw-address

struct V1QueryWithdrawAddress {
  int code;

  struct Item {
    string address;
    string addressTag;
    string note;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Item&) const = default;
  };

  vector<Item> data;
};

// https://huobiapi.github.io/docs/spot/v1/en/#create-a-withdraw-request

struct V1DwWithdrawApiCreate {
  string status;
  int64_t data;
};

}  // namespace cct::schema::huobi

template <>
struct glz::meta<::cct::schema::huobi::MarketHistoryTrade::Trade::TradeData::Direction> {
  using enum ::cct::schema::huobi::MarketHistoryTrade::Trade::TradeData::Direction;
  static constexpr auto value = enumerate(buy, sell);
};

template <>
struct glz::meta<::cct::schema::huobi::V1Orders::Item> {
  using T = ::cct::schema::huobi::V1Orders::Item;
  static constexpr auto value =
      object("symbol", &T::symbol, "field-amount", &T::fieldAmount, "price", &T::price, "type", &T::type, "created-at",
             &T::createdAt, "id", &T::id, "finished-at", &T::finishedAt);
};

template <>
struct glz::meta<::cct::schema::huobi::V1OrderOpenOrders::Item> {
  using T = ::cct::schema::huobi::V1OrderOpenOrders::Item;
  static constexpr auto value = object("symbol", &T::symbol, "amount", &T::amount, "price", &T::price, "filled-amount",
                                       &T::filledAmount, "created-at", &T::createdAt, "id", &T::id, "type", &T::type);
};

template <>
struct glz::meta<::cct::schema::huobi::V1QueryDepositWithdraw::Item> {
  using T = ::cct::schema::huobi::V1QueryDepositWithdraw::Item;
  static constexpr auto value = object("state", &T::state, "currency", &T::currency, "id", &T::id, "amount", &T::amount,
                                       "fee", &T::fee, "updated-at", &T::updatedAt);
};

template <>
struct glz::meta<::cct::schema::huobi::V1OrderOrdersDetail::Data> {
  using T = ::cct::schema::huobi::V1OrderOrdersDetail::Data;
  static constexpr auto value =
      object("state", &T::state, "field-amount", &T::fieldAmount, "field-cash-amount", &T::fieldCashAmount,
             "field-fees", &T::fieldFees, "filled-amount", &T::filledAmount, "filled-cash-amount", &T::filledCashAmount,
             "filled-fees", &T::filledFees);
};