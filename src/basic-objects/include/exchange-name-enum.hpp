#pragma once

#include <cstdint>
#include <string_view>

#include "cct_config.hpp"
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
#ifdef CCT_MSVC
static constexpr std::string_view kSupportedExchanges[] = {"binance", "bithumb", "huobi", "kraken", "kucoin", "upbit"};
#else
static constexpr auto kSupportedExchanges = json::reflect<ExchangeNameEnum>::keys;
#endif

static constexpr int kNbSupportedExchanges = static_cast<int>(std::size(kSupportedExchanges));

static constexpr int kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct