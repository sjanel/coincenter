#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangepublicapi.hpp"
#include "stringhelpers.hpp"
#include "timehelpers.hpp"
#include "tradeoptions.hpp"
#include "wallet.hpp"

namespace cct {

struct CoincenterCmdLineOptions {
  static constexpr std::string_view kDefaultMonitoringIPAddress = "0.0.0.0";  // in Docker, localhost does not work
  static constexpr int kDefaultMonitoringPort = 9091;                         // Prometheus default push port
  static constexpr Duration kDefaultRepeatTime = std::chrono::seconds(1);

  void setLogLevel() const;

  void setLogFile() const;

  static void PrintVersion(std::string_view programName);

  std::string_view dataDir = kDefaultDataDir;

  std::string_view logLevel;
  bool help = false;
  bool version = false;
  bool logFile = false;
  bool noPrint = false;
  std::optional<std::string_view> nosecrets;
  CommandLineOptionalInt repeats;
  Duration repeatTime = kDefaultRepeatTime;

  std::string_view monitoringAddress = kDefaultMonitoringIPAddress;
  std::string_view monitoringUsername;
  std::string_view monitoringPassword;
  int monitoringPort = kDefaultMonitoringPort;
  bool useMonitoring = false;

  std::string_view markets;

  std::string_view orderbook;
  int orderbookDepth = 0;
  std::string_view orderbookCur;

  std::optional<std::string_view> ticker;

  std::string_view conversionPath;

  std::optional<std::string_view> balance;

  std::string_view trade;
  std::string_view tradeAll;
  std::string_view tradeMulti;
  std::string_view tradeMultiAll;
  std::string_view tradePrice;
  bool tradeTimeoutMatch = false;
  Duration tradeTimeout{TradeOptions().maxTradeTime()};
  Duration tradeUpdatePrice{TradeOptions().minTimeBetweenPriceUpdates()};
  bool tradeSim{TradeOptions().isSimulation()};

  std::string_view depositInfo;

  std::optional<std::string_view> openedOrdersInfo;
  std::optional<std::string_view> cancelOpenedOrders;
  std::string_view ordersIds;
  Duration ordersMinAge{};
  Duration ordersMaxAge{};

  std::string_view withdraw;
  std::string_view withdrawFee;

  std::string_view last24hTradedVolume;
  std::string_view lastPrice;

  std::string_view lastTrades;
  int nbLastTrades = api::ExchangePublic::kNbLastTradesDefault;
};

template <class OptValueType>
CommandLineOptionsParser<OptValueType> CreateCoincenterCommandLineOptionsParser() {
  static constexpr TradeOptions kDefaultTradeOptions;
  static constexpr int64_t defaultTradeTimeout =
      std::chrono::duration_cast<std::chrono::seconds>(kDefaultTradeOptions.maxTradeTime()).count();
  static constexpr int64_t minUpdatePriceTime =
      std::chrono::duration_cast<std::chrono::seconds>(kDefaultTradeOptions.minTimeBetweenPriceUpdates()).count();
  static constexpr bool isSimulationModeByDefault = kDefaultTradeOptions.isSimulation();
  static constexpr int64_t kDefaultRepeatDurationSeconds =
      std::chrono::duration_cast<std::chrono::seconds>(CoincenterCmdLineOptions::kDefaultRepeatTime).count();

  // clang-format off
  return CommandLineOptionsParser<OptValueType>(
      {{{{"General", 10}, "--help", 'h', "", "Display this information"}, &OptValueType::help},
       {{{"General", 10}, "--data", 'd', "<path/to/data>", string("Use given 'data' directory instead of the one chosen at build time '")
                                                      .append(kDefaultDataDir).append("'")}, 
                                                      &OptValueType::dataDir},
       {{{"General", 10}, "--log", 'v', "<levelname|0-6>", string("Sets the log level during all execution. "
                                                      "Possible values are: \n(off|critical|error|warning|info|debug|trace) or "
                                                      "(0-6) (default: ").append(log::level::to_string_view(log::get_level()).data()).append(")")}, 
                                                      &OptValueType::logLevel},
       {{{"General", 10}, "--log-file", "", "Log to rotating files instead of stdout / stderr"}, &OptValueType::logFile},
       {{{"General", 10}, "--no-print", "", "Do not print query results in standard output"}, &OptValueType::noPrint},
       {{{"General", 10}, "--no-secrets", "<[exch1,...]>", "Even if present, do not load secrets and do not use private exchanges.\n"
                                                        "If empty list of exchanges, it skips secrets load for all private exchanges"},
                                                        &OptValueType::nosecrets},
       {{{"General", 10}, "--repeat", 'r', "<[n]>", "Indicates how many repeats to perform for mutable data (such as market data)\n"
                                                 "Modifying requests such as trades and withdraws are not impacted by this option. "
                                                 "This is useful for monitoring for instance. 'n' is optional, if not given, will repeat endlessly"},  
                                                 &OptValueType::repeats},
       {{{"General", 10}, "--repeat-time", "<time>", string("Set delay between each repeat (default: ")
                                                    .append(ToString(kDefaultRepeatDurationSeconds)).append("s)")},  
                                                  &OptValueType::repeatTime},
       {{{"General", 10}, "--version", "", "Display program version"}, &OptValueType::version},
       {{{"Public queries", 20}, "--markets", 'm', "<cur1[-cur2][,exch1,...]>", "Print markets involving given currencies for all exchanges, "
                                                                               "or only the specified ones. "
                                                                               "Either a single currency or a full market can be specified."}, 
                                                                                &OptValueType::markets},

       {{{"Public queries", 20}, "--orderbook", 'o', "<cur1-cur2[,exch1,...]>", "Print order book of currency pair for all exchanges offering "
                                                                               "this market, or only for specified exchanges."}, 
                                                                               &OptValueType::orderbook},
       {{{"Public queries", 20}, "--orderbook-depth", "<n>", "Override default depth of order book"}, &OptValueType::orderbookDepth},
       {{{"Public queries", 20}, "--orderbook-cur", "<cur>", "If conversion of cur2 into cur is possible (for each exchange), "
                                                            "prints additional column converted to given asset"}, 
                                                                 &OptValueType::orderbookCur},
       {{{"Public queries", 20}, "--ticker", "<[exch1,...]>", "Print ticker information for all markets for all exchanges,"
                                                           " or only for specified ones"}, 
                                                           &OptValueType::ticker},
       {{{"Public queries", 20}, "--conversion", 'c', "<cur1-cur2[,exch1,...]>", "Print fastest conversion path of 'cur1' to 'cur2' "
                                                                                "for given exchanges if possible"}, 
                                                          &OptValueType::conversionPath},
       {{{"Public queries", 20}, "--volume-day", "<cur1-cur2[,exch1,...]>", "Print last 24h traded volume for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::last24hTradedVolume},
       {{{"Public queries", 20}, "--last-trades", "<cur1-cur2[,exch1,...]>", "Print last trades for market 'cur1'-'cur2' "
                                                                            "for all exchanges (or specified one)"}, 
                                                          &OptValueType::lastTrades},
       {{{"Public queries", 20}, "--last-trades-n", "<n>", string("Change number of last trades to query (default: ").
                                                            append(ToString(api::ExchangePublic::kNbLastTradesDefault)).append(
                                                          ")")}, 
                                                          &OptValueType::nbLastTrades},  
       {{{"Public queries", 20}, "--price", 'p', "<cur1-cur2[,exch1,...]>", "Print last price for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::lastPrice},                                      

       {{{"Private queries", 30}, "--balance", 'b', "<[cur][,exch1,...]>", 
                                                                  "Prints sum of available balance for all selected accounts, "
                                                                  "or all if none given. Optional acronym can be provided, "
                                                                  "in this case a total amount will be printed in this currency "
                                                                  "if conversion is possible."}, 
                                                                  &OptValueType::balance},
       {{{"Private queries", 31}, "--orders-opened", "<cur1-cur2[,exch1,...]>", "Print opened orders matching selection criteria.\n"
                                                               "All cur1, cur2 and exchanges are optional, "
                                                               "returned opened orders will be filtered accordingly."}, 
                                                                &OptValueType::openedOrdersInfo},
       {{{"Private queries", 32}, "--orders-cancel", "<cur1-cur2[,exch1,...]>", "Cancel opened orders matching selection criteria.\n"
                                                               "All cur1, cur2 and exchanges are optional."}, 
                                                                &OptValueType::cancelOpenedOrders},
       {{{"Private queries", 34}, "--orders-id", "<id1,...>", "Only select orders with given ID.\n"
                                                              "One or several IDs can be given, should be comma separated."}, 
                                                              &OptValueType::ordersIds},
       {{{"Private queries", 35}, "--orders-min-age", "<time>", "Only select orders with given minimum age.\n"}, 
                                                                &OptValueType::ordersMinAge},
       {{{"Private queries", 36}, "--orders-max-age", "<time>", "Only select orders with given maximum age.\n"}, 
                                                                &OptValueType::ordersMaxAge},
        {{{"Trade", 40}, "--trade", 't', "<amt[%]cur1-cur2[,exch1,...]>", 
                "Single trade from given start amount on a list of exchanges, "
                "or all that have sufficient balance on cur1 if none provided.\n"
                "Amount can be given as a percentage - in this case the desired percentage "
                "of available amount on matching exchanges will be traded.\n"
                "Orders will be placed prioritizing accounts with largest amounts, at limit price by default."}, 
                &OptValueType::trade},
       {{{"Trade", 40}, "--trade-all", "<cur1-cur2[,exch1,...]>", "Single trade from available amount from given currency on a list of exchanges,"
                                                                 " or all that have some balance on cur1 if none provided\n"
                                                                 "Order will be placed at limit price by default"}, &OptValueType::tradeAll},
       {{{"Trade", 41}, "--singletrade", "<amt[%]cur1-cur2[,exch1,...]>", "Synonym for '--trade'"}, &OptValueType::trade},
       {{{"Trade", 42}, "--multitrade", "<amt[%]cur1-cur2[,exch1,...]>", "Multi trade from given start amount on a list of exchanges,"
                                                                      " or all that have a sufficient balance on cur1 if none provided \n"
                                                                      "Multi trade will first compute fastest path from cur1 to cur2 and "
                                                                     "if possible reach cur2 by launching multiple single trades.\n"
                                                                     "Options are same than for single trade, applied to each step trade.\n"
                                                                     "If multi trade is used in conjonction with single trade, the latter is ignored."}, 
                                                                     &OptValueType::tradeMulti},
       {{{"Trade", 42}, "--multitrade-all", "<cur1-cur2,exchange>", "Multi trade from available amount from given currency on an exchange.\n"
                                                                   "Order will be placed at limit price by default"}, &OptValueType::tradeMultiAll},
       {{{"Trade", 43}, "--trade-strategy", "<maker|nibble|taker>", "Customize the order price strategy of the trade\n"
                                                                  " - 'maker': order price continuously set at limit price (default)\n"
                                                                  " - 'nibble': order price continuously set at limit price + (buy)/- (sell) 1\n"
                                                                  " - 'taker': order price will be at market price, expected to be matched directly"}, 
                                                                  &OptValueType::tradePrice},
       {{{"Trade", 43}, "--trade-timeout", "<time>", string("Adjust trade timeout (default: ")
                                                .append(ToString(defaultTradeTimeout))
                                                .append("s). Remaining orders will be cancelled after the timeout.")}, 
                                                &OptValueType::tradeTimeout},
       {{{"Trade", 43}, "--trade-timeout-match", "", "If after the timeout some amount is still not traded,\n"
                                                    "force match by placing a remaining order at market price\n"}, 
                                                    &OptValueType::tradeTimeoutMatch},
       {{{"Trade", 43}, "--trade-updateprice", "<time>", string("Set the min time allowed between two limit price updates (default: ")
                                                    .append(ToString(minUpdatePriceTime))
                                                    .append("s). Avoids cancelling / placing new orders too often with high volumes "
                                                    "which can be counter productive sometimes.")}, &OptValueType::tradeUpdatePrice},
       {{{"Trade", 44}, "--trade-sim", "", string("Activates simulation mode only (default: ")
                                         .append(isSimulationModeByDefault ? "true" : "false")
                                         .append("). For some exchanges, API can even be queried in this "
                                         "mode to ensure deeper and more realistic trading inputs")}, &OptValueType::tradeSim},
       {{{"Withdraw and deposit", 50}, "--deposit-info", "<cur[,exch1,...]>", "Get deposit wallet information for given currency."
                                                                             " If no exchange accounts are given, will query all of them by default"},
                                                                             &OptValueType::depositInfo},
       {{{"Withdraw and deposit", 50}, "--withdraw", 'w', "<amt cur,from-to>", string("Withdraw amount from exchange 'from' to exchange 'to'."
                                                                         " Amount is gross, including fees. Address and tag will be retrieved"
                                                                         " automatically from destination exchange and should match an entry in '")
                                                                        .append(kDepositAddressesFileName)
                                                                        .append("' file.")}, 
                                                                        &OptValueType::withdraw},
       {{{"Withdraw and deposit", 50}, "--withdraw-fee", "<cur[,exch1,...]>", string("Prints withdraw fees of given currency on all supported exchanges,"
                                                                         " or only for the list of specified ones if provided (comma separated).")}, 
                                                                &OptValueType::withdrawFee},
       {{{"Monitoring", 60}, "--monitoring", "", "Progressively send metrics to external instance provided that it's correctly set up "
                                                "(Prometheus by default). Refer to the README for more information"}, 
                                                       &OptValueType::useMonitoring},
       {{{"Monitoring", 60}, "--monitoring-port", "<port>", string("Specify port of metric gateway instance (default: ")
                                                          .append(ToString(CoincenterCmdLineOptions::kDefaultMonitoringPort)).append(")")}, 
                                                         &OptValueType::monitoringPort},
       {{{"Monitoring", 60}, "--monitoring-ip", "<IPv4>", string("Specify IP (v4) of metric gateway instance (default: ")
                                                        .append(CoincenterCmdLineOptions::kDefaultMonitoringIPAddress).append(")")}, 
                                                         &OptValueType::monitoringAddress},
       {{{"Monitoring", 60}, "--monitoring-user", "<username>", "Specify username of metric gateway instance (default: none)"}, 
                                                                &OptValueType::monitoringUsername},
       {{{"Monitoring", 60}, "--monitoring-pass", "<password>", "Specify password of metric gateway instance (default: none)"}, 
                                                                &OptValueType::monitoringPassword}});
  // clang-format on
}
}  // namespace cct
