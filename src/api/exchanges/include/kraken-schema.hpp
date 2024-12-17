#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <variant>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::kraken {

template <class T>
using has_error_t = decltype(std::declval<T>().error);

// PUBLIC

// https://docs.kraken.com/api/docs/rest-api/get-system-status

struct SystemStatus {
  vector<string> error;

  struct Result {
    string status;
  };

  Result result;
};

// https://docs.kraken.com/api/docs/rest-api/get-asset-info

struct Assets {
  vector<string> error;

  struct Result {
    string altname;
  };

  std::unordered_map<string, Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-tradable-asset-pairs

struct AssetPairs {
  vector<string> error;

  struct Result {
    string base;
    string quote;
    MonetaryAmount ordermin;
    int8_t lot_decimals;
    int8_t pair_decimals;
  };

  std::unordered_map<string, Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-ticker-information

struct Ticker {
  vector<string> error;

  struct Result {
    using AskOrBid = std::array<MonetaryAmount, 3>;

    AskOrBid a;
    AskOrBid b;
    std::array<MonetaryAmount, 2> c;
    std::array<MonetaryAmount, 2> v;
  };

  std::unordered_map<string, Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-order-book

struct Depth {
  vector<string> error;

  struct Result {
    using Item = std::variant<int64_t, string>;

    using Data = std::array<Item, 3>;

    vector<Data> asks;
    vector<Data> bids;
  };

  std::unordered_map<string, Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-recent-trades

struct Trades {
  vector<string> error;

  using Item = std::variant<double, string>;

  using Data = vector<std::array<Item, 7>>;

  std::unordered_map<string, std::variant<Data, string>> result;
};

// PRIVATE

// https://docs.kraken.com/api/docs/rest-api/get-account-balance

struct PrivateBalance {
  vector<string> error;

  std::unordered_map<string, MonetaryAmount> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-deposit-methods

struct DepositMethods {
  vector<string> error;
  struct Data {
    string method;
    string minimum;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Data&) const = default;
  };

  vector<Data> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-deposit-addresses

struct DepositAddresses {
  vector<string> error;

  struct Result {
    string address;
    std::variant<string, int64_t> tag;
    std::variant<string, int64_t> memo;

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Result&) const = default;
  };

  vector<Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-open-orders

// https://docs.kraken.com/api/docs/rest-api/get-closed-orders

struct OpenedOrClosedOrders {
  vector<string> error;

  struct Result {
    struct OpenedOrClosedOrder {
      struct Descr {
        string pair;
        string type;
        MonetaryAmount price;
      };

      Descr descr;
      MonetaryAmount vol;
      MonetaryAmount vol_exec;
      MonetaryAmount price;
      MonetaryAmount cost;
      MonetaryAmount fee;
      double opentm;
      double closetm;
    };

    struct string_hash {
      using hash_type = std::hash<std::string_view>;
      using is_transparent = void;

      std::size_t operator()(const char* str) const { return hash_type{}(str); }
      std::size_t operator()(std::string_view str) const { return hash_type{}(str); }
      std::size_t operator()(const string& str) const { return hash_type{}(str); }
    };

    using OrdersInfoMap = std::unordered_map<string, OpenedOrClosedOrder, string_hash, std::equal_to<>>;

    OrdersInfoMap open;
    OrdersInfoMap closed;
  };

  Result result;
};

// https://docs.kraken.com/api/docs/rest-api/cancel-all-orders

struct CancelAllOrders {
  vector<string> error;

  struct Result {
    int count;
  };

  Result result;
};

// https://docs.kraken.com/api/docs/rest-api/get-status-recent-deposits

struct DepositStatus {
  vector<string> error;

  struct Deposit {
    enum class Status : int8_t {
      Settled,
      Success,
      Failure,
    };

    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Deposit&) const = default;

    Status status;
    string asset;
    MonetaryAmount amount;
    int64_t time;
    string txid;
  };

  vector<Deposit> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-status-recent-withdrawals

struct WithdrawStatus {
  vector<string> error;

  struct Withdraw {
    using trivially_relocatable = is_trivially_relocatable<string>::type;

    auto operator<=>(const Withdraw&) const = default;

    string refid;
    int64_t time;
    string status;
    string asset;
    MonetaryAmount amount;
    MonetaryAmount fee;
  };

  vector<Withdraw> result;
};

// https://docs.kraken.com/api/docs/rest-api/add-order

struct AddOrder {
  vector<string> error;

  struct Result {
    struct Descr {
      string order;
    };

    Descr descr;
    SmallVector<string, 1> txid;
  };

  Result result;
};

// https://docs.kraken.com/api/docs/rest-api/cancel-order

struct CancelOrder {
  vector<string> error;
};

// https://docs.kraken.com/api/docs/rest-api/withdraw-funds

struct Withdraw {
  vector<string> error;

  struct Result {
    string refid;
  };

  Result result;
};

}  // namespace cct::schema::kraken

template <>
struct glz::meta<::cct::schema::kraken::DepositStatus::Deposit::Status> {
  using enum ::cct::schema::kraken::DepositStatus::Deposit::Status;
  static constexpr auto value = enumerate(Settled, Success, Failure);
};