#pragma once

#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeoptions.hpp"

namespace cct {
struct CoincenterCmdLineOptions;

class CoincenterParsedOptions {
 public:
  CoincenterParsedOptions() = default;

  CoincenterParsedOptions(int argc, const char *argv[]);

  MonetaryAmount startTradeAmount;
  CurrencyCode toTradeCurrency;
  PrivateExchangeName tradePrivateExchangeName;
  api::TradeOptions tradeOptions;

  Market marketForOrderBook;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth{};
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  bool balanceForAll{};
  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  MonetaryAmount amountToWithdraw;
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;
  CurrencyCode withdrawFeeCur;
  PublicExchangeNames withdrawFeeExchanges;

  bool noProcess{};

 protected:
  void setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions, const char *programName);
};
}  // namespace cct