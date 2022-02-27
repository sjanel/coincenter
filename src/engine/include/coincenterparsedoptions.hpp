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
  PrivateExchangeNames tradePrivateExchangeNames;
  TradeOptions tradeOptions;

  CurrencyCode marketsCurrency1, marketsCurrency2;
  PublicExchangeNames marketsExchanges;

  Market marketForOrderBook;
  PublicExchangeNames tickerExchanges;
  PublicExchangeNames orderBookExchanges;
  CurrencyCode orderbookCur;

  Market marketForConversionPath;
  PublicExchangeNames conversionPathExchanges;

  PrivateExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  ExchangeSecretsInfo exchangesSecretsInfo;

  CurrencyCode depositCurrency;
  PrivateExchangeNames depositInfoPrivateExchanges;

  PrivateExchangeNames openedOrdersPrivateExchanges;
  OrdersConstraints openedOrdersConstraints;

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

  Market lastPriceMarket;
  PublicExchangeNames lastPriceExchanges;

  std::string_view dataDir;

  MonitoringInfo monitoringInfo;

  bool noProcess = false;
  bool printQueryResults = true;
  bool tickerForAll = false;
  bool balanceForAll = false;
  bool queryOpenedOrders = false;
  bool cancelOpenedOrders = false;
  bool isPercentageTrade = false;
  bool isPercentageWithdraw = false;

  int orderbookDepth = 0;
  int nbLastTrades = 0;
  int repeats = 1;

  Duration repeatTime{};

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
