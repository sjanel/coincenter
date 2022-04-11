#include "coincentercommandtype.hpp"

#include "cct_exception.hpp"

namespace cct {
std::string_view CoincenterCommandTypeToString(CoincenterCommandType type) {
  switch (type) {
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
    case CoincenterCommandType::kTrade:
      return "Trade";
    case CoincenterCommandType::kBuy:
      return "Buy";
    case CoincenterCommandType::kSell:
      return "Sell";
    case CoincenterCommandType::kWithdraw:
      return "Withdraw";
    default:
      throw exception("Unknown command type");
  }
}
}  // namespace cct