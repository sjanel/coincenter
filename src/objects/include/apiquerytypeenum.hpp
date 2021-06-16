#pragma once

namespace cct {
namespace api {
enum class QueryTypeEnum {
  kCurrencies,
  kMarkets,
  kWithdrawalFees,
  kAllOrderBooks,
  kOrderBook,
  kTradedVolume,
  kLastPrice,
  kDepositWallet,
  kNbDecimalsUnitsBithumb
};
}
}  // namespace cct