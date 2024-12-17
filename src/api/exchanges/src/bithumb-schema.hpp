#pragma once

#include <cstdint>
#include <glaze/glaze.hpp>  // IWYU pragma: export
#include <unordered_map>
#include <variant>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "market.hpp"
#include "timepoint-schema.hpp"

namespace cct::schema::bithumb {

template <class T>
using has_status_t = decltype(std::declval<T>().status);

// PUBLIC

// https://apidocs.bithumb.com/reference/%EB%A7%88%EC%BC%93%EC%BD%94%EB%93%9C-%EC%A1%B0%ED%9A%8C
struct V1MarketAllElement {
  Market market;
};

using V1MarketAll = vector<V1MarketAllElement>;

// https://apidocs.bithumb.com/v1.2.0/reference/%ED%98%84%EC%9E%AC%EA%B0%80-%EC%A0%95%EB%B3%B4-%EC%A1%B0%ED%9A%8C-all

struct V1AssetStatus {
  struct CurrencyData {
    int withdrawal_status;
    int deposit_status;
  };

  std::unordered_map<CurrencyCode, CurrencyData> data;

  string status;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%9E%85%EC%B6%9C%EA%B8%88-%EC%A7%80%EC%9B%90-%ED%98%84%ED%99%A9-copy

struct V1NetworkInfo {
  struct CurrencyData {
    CurrencyCode net_type;
    string net_name;

    auto operator<=>(const CurrencyData&) const = default;

    using trivially_relocatable = is_trivially_relocatable<string>::type;
  };

  vector<CurrencyData> data;

  string status;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%ED%98%B8%EA%B0%80-%EC%A0%95%EB%B3%B4-%EC%A1%B0%ED%9A%8C-all

struct OrderbookData {
  struct Order {
    MonetaryAmount price;
    MonetaryAmount quantity;

    auto operator<=>(const Order&) const = default;
  };

  vector<Order> bids;
  vector<Order> asks;

  CurrencyCode order_currency;
  CurrencyCode payment_currency;
};

struct SingleOrderbook {
  OrderbookData data;
  string status;
};

struct MultiOrderbook {
  using Obj = std::variant<string, OrderbookData>;

  std::unordered_map<string, Obj> data;
  string status;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%ED%98%84%EC%9E%AC%EA%B0%80-%EC%A0%95%EB%B3%B4-%EC%A1%B0%ED%9A%8C-all

struct Ticker {
  struct Data {
    string date;
    MonetaryAmount units_traded_24H;
  };

  Data data;
  string status;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%B5%9C%EA%B7%BC-%EC%B2%B4%EA%B2%B0-%EB%82%B4%EC%97%AD

enum class TransactionTypeEnum : int8_t { bid, ask };

struct TransactionHistory {
  struct Data {
    MonetaryAmount units_traded;
    MonetaryAmount price;
    TransactionTypeEnum type;
    string transaction_date;

    auto operator<=>(const Data&) const = default;

    using trivially_relocatable = is_trivially_relocatable<string>::type;
  };

  vector<Data> data;
  string status;
};

// PRIVATE

// https://apidocs.bithumb.com/v1.2.0/reference/%EB%B3%B4%EC%9C%A0%EC%9E%90%EC%82%B0-%EC%A1%B0%ED%9A%8C

struct InfoBalance {
  string status;
  string message;

  using Data = std::unordered_map<string, MonetaryAmount>;

  Data data;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%9E%85%EA%B8%88%EC%A7%80%EA%B0%91-%EC%A3%BC%EC%86%8C-%EC%A1%B0%ED%9A%8C

struct InfoWalletAddress {
  string status;
  string message;

  struct Data {
    string wallet_address;
  };
  Data data;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EA%B1%B0%EB%9E%98-%EC%A3%BC%EB%AC%B8%EB%82%B4%EC%97%AD-%EC%A1%B0%ED%9A%8C

struct InfoOrders {
  string status;
  string message;

  struct OrderDetails {
    std::variant<string, int64_t> order_date;
    string order_id;
    CurrencyCode payment_currency;
    MonetaryAmount units;
    MonetaryAmount units_remaining;
    MonetaryAmount price;
    TransactionTypeEnum type;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const OrderDetails&) const = default;
  };
  vector<OrderDetails> data;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EA%B1%B0%EB%9E%98-%EC%B2%B4%EA%B2%B0%EB%82%B4%EC%97%AD-%EC%A1%B0%ED%9A%8C

struct UserTransactions {
  string status;
  string message;

  struct UserTransaction {
    CurrencyCode order_currency;
    CurrencyCode payment_currency;
    std::variant<string, int64_t> transfer_date;
    string search;
    MonetaryAmount units;
    MonetaryAmount price;
    MonetaryAmount fee;
    MonetaryAmount order_balance;
    MonetaryAmount payment_balance;
    CurrencyCode fee_currency;

    auto operator<=>(const UserTransaction&) const = default;
  };

  vector<UserTransaction> data;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%BD%94%EC%9D%B8-%EC%B6%9C%EA%B8%88%ED%95%98%EA%B8%B0-%EA%B0%9C%EC%9D%B8

struct BtcWithdrawal {
  string status;
  string message;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%A7%80%EC%A0%95%EA%B0%80-%EC%A3%BC%EB%AC%B8%ED%95%98%EA%B8%B0

struct Trade {
  string status;
  string message;
  string order_id;

  struct ExtraStringField {
    string val;
    TimePoint ts;
  };
  struct ExtraIntField {
    int64_t val;
    TimePoint ts;
  };

  struct ExtraData {
    ExtraIntField nbDecimals;
    ExtraStringField minOrderPrice;
    ExtraStringField maxOrderPrice;
    ExtraStringField minOrderSize;
  };

  ExtraData extra_data;  // not part of the API
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EC%A3%BC%EB%AC%B8-%EC%B7%A8%EC%86%8C%ED%95%98%EA%B8%B0

struct TradeCancel {
  string status;
  string message;
};

// https://apidocs.bithumb.com/v1.2.0/reference/%EA%B1%B0%EB%9E%98-%EC%A3%BC%EB%AC%B8%EB%82%B4%EC%97%AD-%EC%83%81%EC%84%B8-%EC%A1%B0%ED%9A%8C

struct InfoOrderDetail {
  string status;
  string message;

  struct Data {
    struct Contract {
      MonetaryAmount units;
      MonetaryAmount price;
      MonetaryAmount fee;
      CurrencyCode fee_currency;

      auto operator<=>(const Contract&) const = default;
    };

    vector<Contract> contract;
  };
  Data data;
};

}  // namespace cct::schema::bithumb

template <>
struct glz::meta<::cct::schema::bithumb::TransactionTypeEnum> {
  using enum ::cct::schema::bithumb::TransactionTypeEnum;
  static constexpr auto value = enumerate(bid, ask);
};