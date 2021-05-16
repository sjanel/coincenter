#pragma once

#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeoptions.hpp"

namespace cct {
struct CoincenterParsedOptions {
  CoincenterParsedOptions(int argc, const char *argv[]);

  MonetaryAmount startTradeAmount;
  CurrencyCode toTradeCurrency;
  PrivateExchangeName tradePrivateExchangeName;
  api::TradeOptions tradeOptions;

  Market marketForOrderBook;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth = 0;
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  MonetaryAmount amountToWithdraw;
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;

  bool noProcess{};
};
}  // namespace cct