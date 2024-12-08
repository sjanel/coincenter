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

}  // namespace cct::schema::huobi

template <>
struct glz::meta<::cct::schema::huobi::MarketHistoryTrade::Trade::TradeData::Direction> {
  using enum ::cct::schema::huobi::MarketHistoryTrade::Trade::TradeData::Direction;
  static constexpr auto value = enumerate(buy, sell);
};