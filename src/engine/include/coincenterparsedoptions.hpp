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
  CoincenterParsedOptions(int argc, const char *argv[]);

  MonetaryAmount startTradeAmount;
  CurrencyCode toTradeCurrency;
  PrivateExchangeName tradePrivateExchangeName;
  api::TradeOptions tradeOptions;

  CurrencyCode marketsCurrency;
  PublicExchangeNames marketsExchanges;

  Market marketForOrderBook;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth{};
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  bool balanceForAll = false;
  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  bool noSecretsForAll = false;
  PublicExchangeNames noSecretsExchanges;

  MonetaryAmount amountToWithdraw;
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;
  CurrencyCode withdrawFeeCur;
  PublicExchangeNames withdrawFeeExchanges;

  Market tradedVolumeMarket;
  PublicExchangeNames tradedVolumeExchanges;

  Market lastPriceMarket;
  PublicExchangeNames lastPriceExchanges;

  string dataDir;

  bool noProcess = false;

 protected:
  void setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions, const char *programName);
};
}  // namespace cct