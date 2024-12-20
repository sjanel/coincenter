#pragma once

#include <cstdint>
#include <glaze/glaze.hpp>

#include "cct_string.hpp"
#include "cct_vector.hpp"

namespace cct::schema::upbit {

template <class T>
using has_error_t = decltype(std::declval<T>().error);

template <class T>
using has_name_t = decltype(std::declval<T>().name);

template <class T>
using has_message_t = decltype(std::declval<T>().message);

// PUBLIC

// https://docs.upbit.com/reference/ticker%ED%98%84%EC%9E%AC%EA%B0%80-%EC%A0%95%EB%B3%B4

struct V1Ticker {
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

}  // namespace cct::schema::upbit

template <>
struct glz::meta<::cct::schema::upbit::V1TradesTick::AskBid> {
  using enum ::cct::schema::upbit::V1TradesTick::AskBid;
  static constexpr auto value = enumerate(ASK, BID);
};