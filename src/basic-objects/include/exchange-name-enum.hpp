#pragma once

#include <cstdint>

#include "cct_json.hpp"

namespace cct {

// Should be sorted alphabetically
#define CCT_EXCHANGE_NAMES binance, bithumb, huobi, kraken, kucoin, upbit

enum class ExchangeNameEnum : int8_t { CCT_EXCHANGE_NAMES };

}  // namespace cct

// To make enum serializable as strings
template <>
struct glz::meta<cct::ExchangeNameEnum> {
  using enum cct::ExchangeNameEnum;

  static constexpr auto value = enumerate(CCT_EXCHANGE_NAMES);
};

#undef CCT_EXCHANGE_NAMES

namespace cct {

/// Ordered list of supported exchange names.
inline constexpr auto kSupportedExchanges = json::reflect<ExchangeNameEnum>::keys;

inline constexpr auto kNbSupportedExchanges = static_cast<int>(std::size(kSupportedExchanges));

inline constexpr auto kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct