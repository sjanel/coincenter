#pragma once

#include <cstdint>
#include <string_view>

#include "cct_json-serialization.hpp"

namespace cct {

enum class CoincenterCommandType : int8_t {
  HealthCheck,
  Currencies,
  Markets,
  Conversion,
  ConversionPath,
  LastPrice,
  Ticker,
  Orderbook,
  LastTrades,
  Last24hTradedVolume,
  WithdrawFees,

  Balance,
  DepositInfo,
  OrdersClosed,
  OrdersOpened,
  OrdersCancel,
  RecentDeposits,
  RecentWithdraws,
  Trade,
  Buy,
  Sell,
  Withdraw,
  DustSweeper,

  MarketData,
  Replay,
  ReplayMarkets,

  Last
};

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type);

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str);

bool IsAnyTrade(CoincenterCommandType type);
}  // namespace cct

template <>
struct glz::meta<::cct::CoincenterCommandType> {
  using enum ::cct::CoincenterCommandType;
  static constexpr auto value = enumerate(HealthCheck, Currencies, Markets, Conversion, ConversionPath, LastPrice,
                                          Ticker, Orderbook, LastTrades, Last24hTradedVolume, WithdrawFees,

                                          Balance, DepositInfo, OrdersClosed, OrdersOpened, OrdersCancel,
                                          RecentDeposits, RecentWithdraws, Trade, Buy, Sell, Withdraw, DustSweeper,

                                          MarketData, Replay, ReplayMarkets,

                                          Last);
};
