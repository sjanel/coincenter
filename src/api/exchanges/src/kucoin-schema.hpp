#pragma once

#include <array>
#include <optional>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::kucoin {

template <class T>
using has_code_t = decltype(std::declval<T>().code);

template <class T>
using has_msg_t = decltype(std::declval<T>().msg);

// PUBLIC

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-service-status

struct V1Status {
  string code;

  struct Data {
    string status;
    string msg;
  } data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-currency-list

struct V3Currencies {
  string code;

  struct Data {
    string currency;

    struct Chain {
      string chainName;
      string chainId;
      MonetaryAmount withdrawalMinFee;
      MonetaryAmount withdrawalMinSize;
      bool isDepositEnabled;
      bool isWithdrawEnabled;

      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Chain&) const = default;
    };

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Data&) const = default;

    std::optional<vector<Chain>> chains;
  };

  vector<Data> data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-symbols-list

struct V2Symbols {
  string code;

  struct V2Symbol {
    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const V2Symbol&) const = default;

    string baseCurrency;
    string quoteCurrency;
    MonetaryAmount baseMinSize;
    MonetaryAmount quoteMinSize;
    MonetaryAmount baseMaxSize;
    MonetaryAmount quoteMaxSize;
    MonetaryAmount baseIncrement;
    MonetaryAmount priceIncrement;
    string feeCurrency;
    bool enableTrading;
  };

  vector<V2Symbol> data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-all-tickers

struct V1AllTickers {
  string code;

  struct Tickers {
    struct Ticker {
      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Ticker&) const = default;

      string symbol;
      std::optional<MonetaryAmount> sell;
      std::optional<MonetaryAmount> buy;
      std::optional<MonetaryAmount> vol;
    };

    vector<Ticker> ticker;
  };

  Tickers data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-part-order-book-aggregated-

struct V1PartOrderBook {
  string code;

  struct Data {
    using AskOrBid = std::array<MonetaryAmount, 2>;

    vector<AskOrBid> asks;
    vector<AskOrBid> bids;
  } data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-24hr-stats

struct V1MarketStats {
  string code;

  struct Data {
    MonetaryAmount vol;
  } data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-trade-histories

struct V1MarketHistories {
  string code;

  struct V1MarketHistory {
    auto operator<=>(const V1MarketHistory&) const = default;

    enum class Side : int8_t { buy, sell };

    MonetaryAmount size;
    MonetaryAmount price;
    uint64_t time;
    Side side;
  };

  vector<V1MarketHistory> data;
};

// https://www.kucoin.com/docs/rest/spot-trading/market-data/get-ticker

struct V1MarketOrderbookLevel1 {
  string code;
  struct Data {
    MonetaryAmount price;
  } data;
};

// PRIVATE

// https://www.kucoin.com/docs/rest/funding/transfer/inner-transfer

struct V1AccountsInnerTransfer {
  string code;
  string msg;
};

// https://www.kucoin.com/docs/rest/account/basic-info/get-account-list-spot-margin-trade_hf

struct V1Accounts {
  string code;
  string msg;

  struct Data {
    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Data&) const = default;

    string type;
    string currency;
    MonetaryAmount available;
    MonetaryAmount balance;
  };

  vector<Data> data;
};

// https://www.kucoin.com/docs/rest/funding/deposit/get-deposit-addresses-v3-

struct V3DepositAddress {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V3DepositAddress&) const = default;

  string address;
  string memo;
};

struct V3DepositAddresses {
  string code;
  string msg;

  vector<V3DepositAddress> data;
};

// https://www.kucoin.com/docs/rest/funding/deposit/create-deposit-address-v3-

struct V3DepositAddressCreate {
  string code;
  string msg;

  V3DepositAddress data;
};

// https://www.kucoin.com/docs/rest/spot-trading/orders/get-order-list

struct V1Orders {
  string code;
  string msg;

  struct Data {
    struct Item {
      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Item&) const = default;

      string symbol;
      int64_t createdAt;
      string id;
      MonetaryAmount dealSize;
      MonetaryAmount price;
      MonetaryAmount size;
      string side;
    };

    vector<Item> items;

  } data;
};

// https://www.kucoin.com/docs/rest/spot-trading/orders/cancel-all-orders

struct V1DeleteOrders {
  string code;
  string msg;

  struct Data {
    vector<string> cancelledOrderIds;
  };

  Data data;
};

// https://www.kucoin.com/docs/rest/funding/deposit/get-deposit-list

struct V1Deposits {
  string code;
  string msg;

  struct Data {
    struct Item {
      enum class Status : int8_t { SUCCESS, PROCESSING, FAILURE };

      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Item&) const = default;

      string currency;
      MonetaryAmount amount;
      int64_t updatedAt;
      Status status;
    };

    vector<Item> items;
  };

  Data data;
};

// https://www.kucoin.com/docs/rest/funding/withdrawals/get-withdrawals-list

struct V1Withdrawals {
  string code;
  string msg;

  struct Data {
    struct Item {
      enum class Status : int8_t { PROCESSING, WALLET_PROCESSING, SUCCESS, FAILURE };

      using trivially_relocatable = is_trivially_relocatable<string>::type;

      auto operator<=>(const Item&) const = default;

      string currency;
      MonetaryAmount amount;
      MonetaryAmount fee;
      int64_t updatedAt;
      Status status;
      string id;
    };

    vector<Item> items;
  };

  Data data;
};

// https://www.kucoin.com/docs/rest/spot-trading/orders/place-order

struct V1OrdersPlace {
  string code;
  string msg;

  struct Data {
    string orderId;
  };

  Data data;
};

// https://www.kucoin.com/docs/rest/spot-trading/orders/cancel-order-by-orderid

struct V1OrderCancel {
  string code;
  string msg;
};

// https://www.kucoin.com/docs/rest/spot-trading/orders/get-order-details-by-orderid

struct V1OrderInfo {
  string code;
  string msg;

  struct Data {
    MonetaryAmount size;
    MonetaryAmount dealSize;
    MonetaryAmount dealFunds;
    bool isActive;
  } data;
};

// https://www.kucoin.com/docs/rest/funding/withdrawals/apply-withdraw-v3-

struct V3ApplyWithdrawal {
  string code;
  string msg;

  struct Data {
    string withdrawalId;
  } data;
};

}  // namespace cct::schema::kucoin

template <>
struct glz::meta<::cct::schema::kucoin::V1MarketHistories::V1MarketHistory::Side> {
  using enum ::cct::schema::kucoin::V1MarketHistories::V1MarketHistory::Side;
  static constexpr auto value = enumerate(buy, sell);
};

template <>
struct glz::meta<::cct::schema::kucoin::V1Deposits::Data::Item::Status> {
  using enum ::cct::schema::kucoin::V1Deposits::Data::Item::Status;
  static constexpr auto value = enumerate(SUCCESS, PROCESSING, FAILURE);
};

template <>
struct glz::meta<::cct::schema::kucoin::V1Withdrawals::Data::Item::Status> {
  using enum ::cct::schema::kucoin::V1Withdrawals::Data::Item::Status;
  static constexpr auto value = enumerate(PROCESSING, WALLET_PROCESSING, SUCCESS, FAILURE);
};