#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json-serialization.hpp"

namespace cct {

#define CCT_COINCENTER_COMMAND_TYPES                                                                               \
  HealthCheck, Currencies, Markets, Conversion, ConversionPath, LastPrice, Ticker, Orderbook, LastTrades,          \
      Last24hTradedVolume, WithdrawFees,                                                                           \
                                                                                                                   \
      Balance, DepositInfo, OrdersClosed, OrdersOpened, OrdersCancel, RecentDeposits, RecentWithdraws, Trade, Buy, \
      Sell, Withdraw, DustSweeper,                                                                                 \
                                                                                                                   \
      MarketData, Replay, ReplayMarkets

enum class CoincenterCommandType : int8_t { CCT_COINCENTER_COMMAND_TYPES };

std::string_view CoincenterCommandTypeToString(CoincenterCommandType coincenterCommandType);

bool IsAnyTrade(CoincenterCommandType coincenterCommandType);
}  // namespace cct

template <>
struct glz::meta<::cct::CoincenterCommandType> {
  using enum ::cct::CoincenterCommandType;
  static constexpr auto value = enumerate(CCT_COINCENTER_COMMAND_TYPES);
};

#undef CCT_COINCENTER_COMMAND_TYPES