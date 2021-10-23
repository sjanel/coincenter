#pragma once

#include <iterator>
#include <string_view>

namespace cct {
static constexpr char kDataDir[] = CCT_DATA_DIR;
static constexpr char kVersion[] = CCT_VERSION;

static constexpr std::string_view kSupportedExchanges[] = {"binance", "bithumb", "huobi", "kraken", "kucoin", "upbit"};

static constexpr int kNbSupportedExchanges =
    static_cast<int>(std::distance(std::begin(kSupportedExchanges), std::end(kSupportedExchanges)));

static constexpr int kTypicalNbPrivateAccounts = kNbSupportedExchanges;

}  // namespace cct
