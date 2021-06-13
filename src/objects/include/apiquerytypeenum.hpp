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
  kDepositWallet,
  kNbDecimalsUnitsBithumb
};
}
}  // namespace cct