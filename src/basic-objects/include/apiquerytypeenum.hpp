#pragma once

#include <cstdint>

#include "cct_json-serialization.hpp"

namespace cct {

#define CCT_QUERY_TYPE \
  currencies, markets, withdrawalFees, allOrderBooks, orderBook, tradedVolume, lastPrice, depositWallet, currencyInfo

enum class QueryType : int8_t { CCT_QUERY_TYPE };

}  // namespace cct

// To make enum serializable as strings
template <>
struct glz::meta<cct::QueryType> {
  using enum cct::QueryType;

  static constexpr auto value = enumerate(CCT_QUERY_TYPE);
};

#undef CCT_QUERY_TYPE