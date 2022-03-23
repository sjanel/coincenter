#pragma once

#include <string_view>

#include "cct_string.hpp"
#include "cct_vector.hpp"
#include "coincentercommand.hpp"
#include "coincenteroptions.hpp"
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

class CoincenterCommands {
 public:
  CoincenterCommands() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  CoincenterCmdLineOptions parseOptions(int argc, const char *argv[]) const;

  MonitoringInfo createMonitoringInfo(std::string_view programName,
                                      const CoincenterCmdLineOptions &cmdLineOptions) const;

  bool setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions);

  MonetaryAmount startTradeAmount;
  MonetaryAmount endTradeAmount;
  CurrencyCode fromTradeCurrency;
  CurrencyCode toTradeCurrency;
  ExchangeNames tradePrivateExchangeNames;
  TradeOptions tradeOptions;

  CurrencyCode marketsCurrency1, marketsCurrency2;
  ExchangeNames marketsExchanges;

  Market marketForOrderBook;
  CurrencyCode orderbookCur;
  ExchangeNames tickerExchanges;
  ExchangeNames orderBookExchanges;

  Market marketForConversionPath;
  ExchangeNames conversionPathExchanges;

  ExchangeNames balancePrivateExchanges;
  CurrencyCode balanceCurrencyCode;

  ExchangeSecretsInfo exchangesSecretsInfo;

  CurrencyCode depositCurrency;
  ExchangeNames depositInfoPrivateExchanges;

  ExchangeNames openedOrdersPrivateExchanges;
  OrdersConstraints openedOrdersConstraints;

  ExchangeNames cancelOpenedOrdersPrivateExchanges;
  OrdersConstraints cancelOpenedOrdersConstraints;

  MonetaryAmount amountToWithdraw;
  ExchangeName withdrawFromExchangeName, withdrawToExchangeName;
  CurrencyCode withdrawFeeCur;
  ExchangeNames withdrawFeeExchanges;

  Market tradedVolumeMarket;
  Market lastTradesMarket;
  Market lastPriceMarket;
  ExchangeNames tradedVolumeExchanges;
  ExchangeNames lastTradesExchanges;
  ExchangeNames lastPriceExchanges;

  Duration repeatTime{};

  int orderbookDepth = 0;
  int nbLastTrades = 0;
  int repeats = 1;

  bool printQueryResults = true;
  bool tickerForAll = false;
  bool balanceForAll = false;
  bool queryOpenedOrders = false;
  bool cancelOpenedOrders = false;
  bool isPercentageTrade = false;
  bool isPercentageWithdraw = false;

 private:
  using Commands = vector<CoincenterCommand>;

  Commands _commands;
};

}  // namespace cct
