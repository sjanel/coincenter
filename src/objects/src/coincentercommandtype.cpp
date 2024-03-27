#include "coincentercommandtype.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <type_traits>

#include "cct_exception.hpp"

namespace cct {
namespace {
constexpr std::string_view kCommandTypeNames[] = {
    "HealthCheck",    "Currencies",      "Markets",      "Conversion",   "ConversionPath",
    "LastPrice",      "Ticker",          "Orderbook",    "LastTrades",   "Last24hTradedVolume",
    "WithdrawFees",

    "Balance",        "DepositInfo",     "OrdersClosed", "OrdersOpened", "OrdersCancel",
    "RecentDeposits", "RecentWithdraws", "Trade",        "Buy",          "Sell",
    "Withdraw",       "DustSweeper",     "MarketData",   "Replay",       "ReplayMarkets"};

static_assert(std::size(kCommandTypeNames) == static_cast<std::size_t>(CoincenterCommandType::kLast));
}  // namespace

std::string_view CoincenterCommandTypeToString(CoincenterCommandType type) {
  const auto intValue = static_cast<std::underlying_type_t<CoincenterCommandType>>(type);
  if (intValue < decltype(intValue){} ||
      intValue >= static_cast<std::underlying_type_t<CoincenterCommandType>>(CoincenterCommandType::kLast)) {
    throw exception("Unknown command type {}", intValue);
  }
  return kCommandTypeNames[intValue];
}

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str) {
  const auto cmdIt = std::ranges::find(kCommandTypeNames, str);
  if (cmdIt == std::end(kCommandTypeNames)) {
    throw exception("Unknown command type {}", str);
  }
  return static_cast<CoincenterCommandType>(cmdIt - std::begin(kCommandTypeNames));
}

bool IsAnyTrade(CoincenterCommandType type) {
  switch (type) {
    case CoincenterCommandType::kTrade:
      [[fallthrough]];
    case CoincenterCommandType::kBuy:
      [[fallthrough]];
    case CoincenterCommandType::kSell:
      return true;
    default:
      return false;
  }
}
}  // namespace cct
