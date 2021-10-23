#pragma once

#include "cct_file.hpp"

namespace cct {
/// Cache files (JSON / CSV files used as caches, will be updated at normal ending of program with latest values)
/// TODO: check if we could use a DB here instead
static constexpr File kBithumbDecimalsCache(File::Type::kCache, "bithumbdecimalscache.json",
                                            File::IfNotFound::kNoThrow);
static constexpr File kFiatCache(File::Type::kCache, "fiatcache.json", File::IfNotFound::kNoThrow);
static constexpr File kKrakenWithdrawalFees(File::Type::kCache, "krakenwithdrawalfees.csv", File::IfNotFound::kThrow);
static constexpr File kKrakenWithdrawInfo(File::Type::kCache, "krakenwithdrawinfo.json", File::IfNotFound::kNoThrow);
static constexpr File kRatesCache(File::Type::kCache, "ratescache.json", File::IfNotFound::kNoThrow);

/// Secret files (private files containing sensitive information)

/// File containing all validated external addresses.
/// It should be a json file with this format:
/// {
///   "exchangeName1": {"BTC": "btcAddress", "XRP": "xrpAdress,xrpTag", "EOS": "eosAddress,eosTag"},
///   "exchangeName2": {...}
/// }
/// In case crypto contains an additional "tag", "memo" or other, it will be placed after the ',' in the address
/// field.
static constexpr File kDepositAddresses(File::Type::kSecret, "depositaddresses.json", File::IfNotFound::kNoThrow);
static constexpr File kSecretTest(File::Type::kSecret, "secret_test.json", File::IfNotFound::kThrow);
static constexpr File kSecret(File::Type::kSecret, "secret.json", File::IfNotFound::kNoThrow);
static constexpr File kThirdPartySecret(File::Type::kSecret, "thirdparty_secret.json", File::IfNotFound::kNoThrow);

/// Static files (public files loaded at startup of program, not supposed to be changed over time)
static constexpr File kCurrencyAcronymsTranslator(File::Type::kStatic, "currencyacronymtranslator.json",
                                                  File::IfNotFound::kThrow);
static constexpr File kExchangeConfig(File::Type::kStatic, "exchangeconfig.json", File::IfNotFound::kNoThrow);
static constexpr File kStableCoins(File::Type::kStatic, "stablecoins.json", File::IfNotFound::kThrow);
static constexpr File kWithdrawFees(File::Type::kStatic, "withdrawfees.json", File::IfNotFound::kThrow);
}  // namespace cct