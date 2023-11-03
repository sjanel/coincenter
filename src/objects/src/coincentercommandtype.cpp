#include "coincentercommandtype.hpp"

#include <string_view>

#include "cct_exception.hpp"

namespace cct {
std::string_view CoincenterCommandTypeToString(CoincenterCommandType type) {
  switch (type) {
    case CoincenterCommandType::kHealthCheck:
      return "HealthCheck";
    case CoincenterCommandType::kMarkets:
      return "Markets";
    case CoincenterCommandType::kConversionPath:
      return "ConversionPath";
    case CoincenterCommandType::kLastPrice:
      return "LastPrice";
    case CoincenterCommandType::kTicker:
      return "Ticker";
    case CoincenterCommandType::kOrderbook:
      return "Orderbook";
    case CoincenterCommandType::kLastTrades:
      return "LastTrades";
    case CoincenterCommandType::kLast24hTradedVolume:
      return "Last24hTradedVolume";
    case CoincenterCommandType::kWithdrawFee:
      return "WithdrawFee";

    case CoincenterCommandType::kBalance:
      return "Balance";
    case CoincenterCommandType::kDepositInfo:
      return "DepositInfo";
    case CoincenterCommandType::kOrdersOpened:
      return "OrdersOpened";
    case CoincenterCommandType::kOrdersCancel:
      return "OrdersCancel";
    case CoincenterCommandType::kRecentDeposits:
      return "RecentDeposits";
    case CoincenterCommandType::kRecentWithdraws:
      return "RecentWithdraws";
    case CoincenterCommandType::kTrade:
      return "Trade";
    case CoincenterCommandType::kBuy:
      return "Buy";
    case CoincenterCommandType::kSell:
      return "Sell";
    case CoincenterCommandType::kWithdrawApply:
      return "Withdraw";
    case CoincenterCommandType::kDustSweeper:
      return "DustSweeper";
    default:
      throw exception("Unknown command type");
  }
}

CoincenterCommandType CoincenterCommandTypeFromString(std::string_view str) {
  if (str == "HealthCheck") {
    return CoincenterCommandType::kHealthCheck;
  }
  if (str == "Markets") {
    return CoincenterCommandType::kMarkets;
  }
  if (str == "ConversionPath") {
    return CoincenterCommandType::kConversionPath;
  }
  if (str == "LastPrice") {
    return CoincenterCommandType::kLastPrice;
  }
  if (str == "Ticker") {
    return CoincenterCommandType::kTicker;
  }
  if (str == "Orderbook") {
    return CoincenterCommandType::kOrderbook;
  }
  if (str == "LastTrades") {
    return CoincenterCommandType::kLastTrades;
  }
  if (str == "Last24hTradedVolume") {
    return CoincenterCommandType::kLast24hTradedVolume;
  }
  if (str == "WithdrawFee") {
    return CoincenterCommandType::kWithdrawFee;
  }

  if (str == "Balance") {
    return CoincenterCommandType::kBalance;
  }
  if (str == "DepositInfo") {
    return CoincenterCommandType::kDepositInfo;
  }
  if (str == "OrdersOpened") {
    return CoincenterCommandType::kOrdersOpened;
  }
  if (str == "OrdersCancel") {
    return CoincenterCommandType::kOrdersCancel;
  }
  if (str == "RecentDeposits") {
    return CoincenterCommandType::kRecentDeposits;
  }
  if (str == "RecentWithdraws") {
    return CoincenterCommandType::kRecentWithdraws;
  }
  if (str == "Trade") {
    return CoincenterCommandType::kTrade;
  }
  if (str == "Buy") {
    return CoincenterCommandType::kBuy;
  }
  if (str == "Sell") {
    return CoincenterCommandType::kSell;
  }
  if (str == "Withdraw") {
    return CoincenterCommandType::kWithdrawApply;
  }
  if (str == "DustSweeper") {
    return CoincenterCommandType::kDustSweeper;
  }
  throw exception("Unknown command type {}", str);
}
}  // namespace cct