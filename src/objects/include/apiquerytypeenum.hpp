#pragma once

#include <cstdint>

namespace cct::api {
enum QueryType : int8_t {};

static constexpr QueryType kCurrencies{0};
static constexpr QueryType kMarkets{1};
static constexpr QueryType kWithdrawalFees{2};
static constexpr QueryType kAllOrderBooks{3};
static constexpr QueryType kOrderBook{4};
static constexpr QueryType kTradedVolume{5};
static constexpr QueryType kLastPrice{6};
static constexpr QueryType kDepositWallet{7};
static constexpr QueryType kCurrencyInfoBithumb{8};

static constexpr QueryType kQueryTypeMax{9};

}  // namespace cct::api