#pragma once

namespace cct {
namespace api {
enum class QueryTypeEnum {
  kCurrencies,
  kMarkets,
  kWithdrawalFees,
  kAllOrderBooks,
  kOrderBook,
  kAccountBalance,
  kDepositWallet,
  kNbDecimalsUnitsBithumb
};
}
}  // namespace cct