#pragma once

#include <string_view>

namespace cct {

/// File containing all validated external addresses.
/// It should be a json file with this format:
/// {
///   "exchangeName1": {"BTC": "btcAddress", "XRP": "xrpAddress,xrpTag", "EOS": "eosAddress,eosTag"},
///   "exchangeName2": {...}
/// }
/// In case crypto contains an additional "tag", "memo" or other, it will be placed after the ',' in the address
/// field.
inline constexpr std::string_view kDepositAddressesFileName = "depositaddresses.json";

}  // namespace cct