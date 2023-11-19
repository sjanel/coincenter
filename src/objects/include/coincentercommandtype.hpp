#pragma once

#include <cstdint>
#include <string_view>

namespace cct {
enum class CoincenterCommandType : int8_t {
  kHealthCheck,
  kCurrencies,
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
  kRecentDeposits,
  kRecentWithdraws,
  kTrade,
  kBuy,
  kSell,
  kWithdrawApply,
  kDustSweeper,
};

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type);

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str);

bool IsAnyTrade(CoincenterCommandType type);
}  // namespace cct