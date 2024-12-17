#pragma once

#include <array>
#include <cstdint>
#include <unordered_map>
#include <variant>

#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct::schema::kraken {

template <class T>
using has_error_t = decltype(std::declval<T>().error);

// PUBLIC

// https://docs.kraken.com/api/docs/rest-api/get-system-status

struct PublicSystemStatus {
  vector<string> error;

  struct Result {
    string status;
  };

  Result result;
};

// https://docs.kraken.com/api/docs/rest-api/get-asset-info

struct PublicAssets {
  vector<string> error;

  struct Result {
    string altname;
  };

  std::unordered_map<string, Result> result;
};

// https://docs.kraken.com/api/docs/rest-api/get-tradable-asset-pairs

struct PublicAssetPairs {
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

struct PublicTicker {
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

struct PublicDepth {
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

struct PublicTrades {
  vector<string> error;

  using Item = std::variant<double, string>;

  using Data = vector<std::array<Item, 7>>;

  std::unordered_map<string, std::variant<Data, string>> result;
};

}  // namespace cct::schema::kraken
