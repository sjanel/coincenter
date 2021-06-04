#pragma once

#include <iterator>
#include <string_view>

namespace cct {
static constexpr char kDataPath[] = CCT_DATA_PATH;
static constexpr char kConfigPath[] = CCT_CONFIG_PATH;
static constexpr char kVersion[] = CCT_VERSION;

static constexpr std::string_view kSupportedExchanges[] = {"binance", "bithumb", "huobi", "kraken", "upbit"};

static constexpr int kNbSupportedExchanges =
    std::distance(std::begin(kSupportedExchanges), std::end(kSupportedExchanges));

static constexpr int kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct
