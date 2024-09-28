#pragma once

#include <algorithm>
#include <iterator>
#include <string_view>

namespace cct {
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

static constexpr std::string_view kSupportedExchanges[] = {"binance", "bithumb", "huobi", "kraken", "kucoin", "upbit"};

static_assert(std::ranges::is_sorted(kSupportedExchanges));

static constexpr int kNbSupportedExchanges = static_cast<int>(std::size(kSupportedExchanges));

static constexpr int kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct
