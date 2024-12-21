#pragma once

#include <cstdint>
#include <glaze/glaze.hpp>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"
#include "request-retry.hpp"

namespace cct::schema::upbit {

struct Error {
  struct Sub {
    std::variant<int64_t, string> name;
    string message;
  } error;
};

template <class T>
std::pair<T, Error> GetOrValueInitialized(
    RequestRetry& requestRetry, std::string_view endpoint,
    std::function<void(CurlOptions&)> postDataUpdateFunc = [](CurlOptions&) {}) {
  using VarT = std::variant<schema::upbit::Error, T>;

  VarT varT = requestRetry.query<VarT>(
      endpoint,
      [](const VarT& response) {
        return std::visit(
            [](auto&& arg) -> RequestRetry::Status {
              using V = std::decay_t<decltype(arg)>;
              if constexpr (std::is_same_v<V, Error>) {
                std::string_view msg = arg.error.message;
                std::visit(
                    [msg](auto&& arg) {
                      using U = std::decay_t<decltype(arg)>;
                      if constexpr (std::is_same_v<U, int64_t> || std::is_same_v<U, string>) {
                        log::warn("Upbit error ({}, '{}')", arg, msg);
                      } else {
                        static_assert(std::is_same_v<U, int64_t> || std::is_same_v<U, string>,
                                      "non-exhaustive visitor!");
                      }
                    },
                    arg.error.name);

                return RequestRetry::Status::kResponseError;
              } else if constexpr (std::is_same_v<V, T>) {
                return RequestRetry::Status::kResponseOK;
              } else {
                static_assert(std::is_same_v<V, Error> || std::is_same_v<V, T>, "non-exhaustive visitor!");
              }
            },
            response);
      },
      postDataUpdateFunc);

  return std::visit(
      [](auto&& arg) -> std::pair<T, Error> {
        using V = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<V, Error>) {
          return {T{}, std::move(arg)};
        } else if constexpr (std::is_same_v<V, T>) {
          return {std::move(arg), Error{}};
        } else {
          static_assert(std::is_same_v<V, Error> || std::is_same_v<V, T>, "non-exhaustive visitor!");
        }
      },
      varT);
}
// PUBLIC

// https://docs.upbit.com/reference/ticker%ED%98%84%EC%9E%AC%EA%B0%80-%EC%A0%95%EB%B3%B4

struct V1Ticker {
  auto operator<=>(const V1Ticker&) const = default;
  int64_t timestamp;
};

using V1Tickers = vector<V1Ticker>;

// https://docs.upbit.com/reference/%EB%A7%88%EC%BC%93-%EC%BD%94%EB%93%9C-%EC%A1%B0%ED%9A%8C

struct V1Market {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1Market&) const = default;

  string market;
  string market_warning;
};

using V1MarketAll = vector<V1Market>;

// https://docs.upbit.com/reference/%ED%98%B8%EA%B0%80-%EC%A0%95%EB%B3%B4-%EC%A1%B0%ED%9A%8C

struct V1OrderBook {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1OrderBook&) const = default;

  struct Unit {
    auto operator<=>(const Unit&) const = default;

    double ask_price;
    double bid_price;
    double ask_size;
    double bid_size;
  };

  string market;
  vector<Unit> orderbook_units;
};

using V1Orderbooks = vector<V1OrderBook>;

// https://docs.upbit.com/reference/%EC%9D%BCday-%EC%BA%94%EB%93%A4-1

struct V1CandleDay {
  auto operator<=>(const V1CandleDay&) const = default;

  double candle_acc_trade_volume;
};

using V1CandlesDay = vector<V1CandleDay>;

// https://docs.upbit.com/reference/%EC%B5%9C%EA%B7%BC-%EC%B2%B4%EA%B2%B0-%EB%82%B4%EC%97%AD

struct V1TradesTick {
  auto operator<=>(const V1TradesTick&) const = default;

  enum class AskBid : int8_t { ASK, BID };

  double trade_volume;
  double trade_price;
  int64_t timestamp;
  AskBid ask_bid;
};

using V1TradesTicks = vector<V1TradesTick>;

// PRIVATE

// https://docs.upbit.com/reference/open-api-%ED%82%A4-%EB%A6%AC%EC%8A%A4%ED%8A%B8-%EC%A1%B0%ED%9A%8C

struct V1ApiKey {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1ApiKey&) const = default;

  string access_key;
  string expire_at;
};

using V1ApiKeys = SmallVector<V1ApiKey, 1>;

struct V1StatusWallet {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1StatusWallet&) const = default;

  enum class WalletState : int8_t { working, withdraw_only, deposit_only, paused, unsupported };

  string currency;
  string net_type;
  WalletState wallet_state;
};

using V1StatusWallets = vector<V1StatusWallet>;

// https://docs.upbit.com/reference/%EC%A0%84%EC%B2%B4-%EA%B3%84%EC%A2%8C-%EC%A1%B0%ED%9A%8C

struct V1Account {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1Account&) const = default;

  string currency;
  MonetaryAmount balance;
  MonetaryAmount locked;
};

using V1Accounts = vector<V1Account>;

// https://docs.upbit.com/reference/%EA%B0%9C%EB%B3%84-%EC%9E%85%EA%B8%88-%EC%A3%BC%EC%86%8C-%EC%A1%B0%ED%9A%8C

struct V1DepositCoinAddress {
  string currency;
  string net_type;
  string deposit_address;
  std::optional<string> secondary_address;
};

// https://docs.upbit.com/reference/%EC%9E%85%EA%B8%88-%EC%A3%BC%EC%86%8C-%EC%83%9D%EC%84%B1-%EC%9A%94%EC%B2%AD

struct V1DepositsGenerateCoinAddress {
  bool success;
  string message;
};

// https://docs.upbit.com/reference/%EB%8C%80%EA%B8%B0-%EC%A3%BC%EB%AC%B8-%EC%A1%B0%ED%9A%8C
// https://docs.upbit.com/reference/%EC%A2%85%EB%A3%8C-%EC%A3%BC%EB%AC%B8-%EC%A1%B0%ED%9A%8C

struct V1Order {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1Order&) const = default;

  string uuid;
  string market;
  string created_at;
  string side;
  MonetaryAmount price;
  MonetaryAmount executed_volume;
  MonetaryAmount remaining_volume;
};

using V1Orders = vector<V1Order>;

// https://docs.upbit.com/reference/%EC%9E%85%EA%B8%88-%EB%A6%AC%EC%8A%A4%ED%8A%B8-%EC%A1%B0%ED%9A%8C

struct V1Deposit {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1Deposit&) const = default;

  enum class State : int8_t { PROCESSING, REFUNDING, ACCEPTED, CANCELLED, REJECTED, TRAVEL_RULE_SUSPECTED, REFUNDED };

  string currency;
  string txid;
  string created_at;
  std::optional<string> done_at;
  MonetaryAmount amount;
  State state;
};

using V1Deposits = vector<V1Deposit>;

// https://docs.upbit.com/reference/%EC%A0%84%EC%B2%B4-%EC%B6%9C%EA%B8%88-%EC%A1%B0%ED%9A%8C

struct V1Withdraw {
  using trivially_relocatable = is_trivially_relocatable<string>::type;

  auto operator<=>(const V1Withdraw&) const = default;

  // In earlier versions of Upbit API, 'CANCELED' was written with this typo.
  // Let's support both spellings to avoid issues.
  enum class State : int8_t { WAITING, PROCESSING, DONE, FAILED, CANCELLED, CANCELED, REJECTED };

  string currency;
  string txid;
  string created_at;
  std::optional<string> done_at;
  MonetaryAmount amount;
  MonetaryAmount fee;
  State state;
};

using V1Withdraws = vector<V1Withdraw>;

// https://docs.upbit.com/reference/%EC%A3%BC%EB%AC%B8%ED%95%98%EA%B8%B0

struct V1SingleOrder {
  string uuid;
  string state;
  std::optional<MonetaryAmount> paid_fee;

  struct Trade {
    auto operator<=>(const Trade&) const = default;

    MonetaryAmount volume;
    MonetaryAmount price;
    MonetaryAmount funds;
  };

  std::optional<vector<Trade>> trades;
};

// https://docs.upbit.com/reference/%EC%B6%9C%EA%B8%88-%EA%B0%80%EB%8A%A5-%EC%A0%95%EB%B3%B4

struct V1WithdrawChance {
  struct Currency {
    MonetaryAmount withdraw_fee;
  } currency;
};

// https://docs.upbit.com/reference/%EB%94%94%EC%A7%80%ED%84%B8%EC%9E%90%EC%82%B0-%EC%B6%9C%EA%B8%88%ED%95%98%EA%B8%B0

struct V1WithdrawsCoin {
  string uuid;
};

}  // namespace cct::schema::upbit

template <>
struct glz::meta<::cct::schema::upbit::V1TradesTick::AskBid> {
  using enum ::cct::schema::upbit::V1TradesTick::AskBid;
  static constexpr auto value = enumerate(ASK, BID);
};

template <>
struct glz::meta<::cct::schema::upbit::V1StatusWallet::WalletState> {
  using enum ::cct::schema::upbit::V1StatusWallet::WalletState;
  static constexpr auto value = enumerate(working, withdraw_only, deposit_only, paused, unsupported);
};

template <>
struct glz::meta<::cct::schema::upbit::V1Deposit::State> {
  using enum ::cct::schema::upbit::V1Deposit::State;
  static constexpr auto value =
      enumerate(PROCESSING, REFUNDING, ACCEPTED, CANCELLED, REJECTED, TRAVEL_RULE_SUSPECTED, REFUNDED);
};

template <>
struct glz::meta<::cct::schema::upbit::V1Withdraw::State> {
  using enum ::cct::schema::upbit::V1Withdraw::State;
  static constexpr auto value = enumerate(WAITING, PROCESSING, DONE, FAILED, CANCELLED, CANCELED, REJECTED);
};