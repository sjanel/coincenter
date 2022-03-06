#pragma once

#include <cstdint>
#include <string_view>

namespace cct {
enum class CoincenterCommandType : int8_t {
  kMarkets,
  kConversionPath,
  kLastPrice,
  kTicker,
  kOrderbook,
  kLastTrades,
  kLast24hTradedVolume,
  kWithdrawFee,

  kBalance,
  kDepositInfo,
  kOrdersOpened,
  kOrdersCancel,
  kTrade,
  kBuy,
  kSell,
  kWithdraw,
  kDustSweeper,
};

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type);
}  // namespace cct