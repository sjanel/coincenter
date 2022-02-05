#pragma once

#include <string_view>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "exchangesecretsinfo.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "monitoringinfo.hpp"
#include "ordersconstraints.hpp"
#include "timedef.hpp"
#include "tradeoptions.hpp"

namespace cct {
struct CoincenterCmdLineOptions;

class CoincenterParsedOptions {
 public:
  /// Parse arguments and store the overriden values in this object.
  CoincenterParsedOptions(int argc, const char *argv[]);

  MonetaryAmount startTradeAmount;
  MonetaryAmount endTradeAmount;
  CurrencyCode fromTradeCurrency;
  CurrencyCode toTradeCurrency;
  bool isPercentageTrade = false;
  PrivateExchangeNames tradePrivateExchangeNames;
  TradeOptions tradeOptions;

  CurrencyCode marketsCurrency1, marketsCurrency2;
  PublicExchangeNames marketsExchanges;

  Market marketForOrderBook;
  bool tickerForAll = false;
  PublicExchangeNames tickerExchanges;
  PublicExchangeNames orderBookExchanges;
  int orderbookDepth = 0;
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  bool balanceForAll = false;
  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  ExchangeSecretsInfo exchangesSecretsInfo;

  CurrencyCode depositCurrency;
  PrivateExchangeNames depositInfoPrivateExchanges;

  bool queryOpenedOrders = false;
  PrivateExchangeNames openedOrdersPrivateExchanges;
  OrdersConstraints openedOrdersConstraints;

  bool cancelOpenedOrders = false;
  PrivateExchangeNames cancelOpenedOrdersPrivateExchanges;
  OrdersConstraints cancelOpenedOrdersConstraints;

  MonetaryAmount amountToWithdraw;
  PrivateExchangeName withdrawFromExchangeName, withdrawToExchangeName;
  CurrencyCode withdrawFeeCur;
  PublicExchangeNames withdrawFeeExchanges;

  Market tradedVolumeMarket;
  PublicExchangeNames tradedVolumeExchanges;

  Market lastTradesMarket;
  PublicExchangeNames lastTradesExchanges;
  int nbLastTrades;

  Market lastPriceMarket;
  PublicExchangeNames lastPriceExchanges;

  std::string_view dataDir;

  MonitoringInfo monitoringInfo;

  bool noProcess = false;
  bool printQueryResults;
  int repeats = 1;
  Duration repeatTime;

  std::string_view programName() const { return _programName; }

 protected:
  /// Constructor to be called for programs extending the command line options of 'coincenter'.
  /// Indeed, it's not possible to call the constructor with argv as it will contain some unknown arguments from higher
  /// level program
  CoincenterParsedOptions() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  void setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions);

 private:
  string _programName;
};

}  // namespace cct
