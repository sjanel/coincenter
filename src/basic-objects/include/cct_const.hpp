#pragma once

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <string_view>

#include "cct_json-serialization.hpp"

namespace cct {

enum class SupportedExchangeName : int8_t { binance, bithumb, huobi, kraken, kucoin, upbit };

static constexpr std::string_view kDefaultDataDir = CCT_DATA_DIR;

/// File containing all validated external addresses.
/// It should be a json file with this format:
/// {
///   "exchangeName1": {"BTC": "btcAddress", "XRP": "xrpAddress,xrpTag", "EOS": "eosAddress,eosTag"},
///   "exchangeName2": {...}
/// }
/// In case crypto contains an additional "tag", "memo" or other, it will be placed after the ',' in the address
/// field.
static constexpr std::string_view kDepositAddressesFileName = "depositaddresses.json";

/// File containing exchange configuration.
/// It has a particular format, with default values which can optionally be overriden by exchange name.
/// See ExchangeConfig in exchange-config.hpp.
static constexpr std::string_view kExchangeConfigFileName = "exchangeconfig.json";

}  // namespace cct

// To make enum serializable as strings
template <>
struct glz::meta<cct::SupportedExchangeName> {
  using enum cct::SupportedExchangeName;

  static constexpr auto value = enumerate(binance, bithumb, huobi, kraken, kucoin, upbit);
};

namespace cct {

/// Ordered list of supported exchange names.
static constexpr auto kSupportedExchanges = json::reflect<SupportedExchangeName>::keys;

static_assert(std::ranges::is_sorted(kSupportedExchanges));

static constexpr int kNbSupportedExchanges = static_cast<int>(std::size(kSupportedExchanges));

static constexpr int kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct