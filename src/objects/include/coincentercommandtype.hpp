#pragma once

#include <cstdint>
#include <string_view>

namespace cct {

enum class CoincenterCommandType : int8_t {
  kHealthCheck,
  kCurrencies,
  kMarkets,
  kConversion,
  kConversionPath,
  kLastPrice,
  kTicker,
  kOrderbook,
  kLastTrades,
  kLast24hTradedVolume,
  kWithdrawFees,

  kBalance,
  kDepositInfo,
  kOrdersClosed,
  kOrdersOpened,
  kOrdersCancel,
  kRecentDeposits,
  kRecentWithdraws,
  kTrade,
  kBuy,
  kSell,
  kWithdrawApply,
  kDustSweeper,

  kMarketData,
  kReplay,
  kReplayMarkets,

  kLast
};

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type);

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str);

bool IsAnyTrade(CoincenterCommandType type);
}  // namespace cct
