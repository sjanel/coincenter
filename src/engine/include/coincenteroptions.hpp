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

  string dataDir = kDefaultDataDir;

  string logLevel;
  bool help = false;
  bool version = false;
  bool logFile = false;
  bool noPrint = false;
  std::optional<string> nosecrets;
  CommandLineOptionalInt repeats;
  Duration repeat_time = kDefaultRepeatTime;

  string monitoring_address = string(kDefaultMonitoringIPAddress);
  string monitoring_username;
  string monitoring_password;
  int monitoring_port = kDefaultMonitoringPort;
  bool useMonitoring = false;

  string markets;

  string orderbook;
  int orderbook_depth = 0;
  string orderbook_cur;

  std::optional<string> ticker;

  string conversion_path;

  std::optional<string> balance;
  string balance_cur{CurrencyCode::kNeutral.str()};

  string trade;
  string trade_all;
  string trade_multi;
  string trade_multi_all;
  string trade_price;
  bool trade_timeout_match = false;
  Duration trade_timeout{TradeOptions().maxTradeTime()};
  Duration trade_updateprice{TradeOptions().minTimeBetweenPriceUpdates()};
  bool trade_sim{TradeOptions().isSimulation()};

  string deposit_info;

  std::optional<string> opened_orders_info;
  Duration orders_min_age{};
  Duration orders_max_age{};

  string withdraw;
  string withdraw_fee;

  string last24hTradedVolume;
  string lastPrice;

  string lastTrades;
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
      {{{{"General", 1}, "--help", 'h', "", "Display this information"}, &OptValueType::help},
       {{{"General", 1}, "--version", "", "Display program version"}, &OptValueType::version},
       {{{"General", 1}, "--data", 'd', "<path/to/data>", string("Use given 'data' directory instead of the one chosen at build time '")
                                                      .append(kDefaultDataDir).append("'")}, 
                                                      &OptValueType::dataDir},
       {{{"General", 1}, "--log", 'v', "<levelname|0-6>", string("Sets the log level during all execution. "
                                                      "Possible values are: \n(off|critical|error|warning|info|debug|trace) or "
                                                      "(0-6) (default: ").append(log::level::to_string_view(log::get_level()).data()).append(")")}, 
                                                      &OptValueType::logLevel},
       {{{"General", 1}, "--log-file", "", "Log to rotating files instead of stdout / stderr"}, &OptValueType::logFile},
       {{{"General", 1}, "--no-print", "", "Do not print query results in standard output"}, &OptValueType::noPrint},
       {{{"General", 1}, "--no-secrets", "[exch1,...]", "Even if present, do not load secrets and do not use private exchanges.\n"
                                                        "If empty list of exchanges, it skips secrets load for all private exchanges"},
                                                        &OptValueType::nosecrets},
       {{{"General", 1}, "--repeat", 'r', "[n]", "Indicates how many repeats to perform for mutable data (such as market data)\n"
                                                 "Modifying requests such as trades and withdraws are not impacted by this option. "
                                                 "This is useful for monitoring for instance. 'n' is optional, if not given, will repeat endlessly"},  
                                                 &OptValueType::repeats},
       {{{"General", 1}, "--repeat-time", "<time>", string("Set delay between each repeat (default: ")
                                                    .append(ToString<string>(kDefaultRepeatDurationSeconds)).append("s)")},  
                                                  &OptValueType::repeat_time},
       {{{"Public queries", 2}, "--markets", 'm', "<cur1[-cur2][,exch1,...]>", "Print markets involving given currencies for all exchanges, "
                                                                               "or only the specified ones. "
                                                                               "Either a single currency or a full market can be specified."}, 
                                                                                &OptValueType::markets},

       {{{"Public queries", 2}, "--orderbook", 'o', "<cur1-cur2[,exch1,...]>", "Print order book of currency pair for all exchanges offering "
                                                                               "this market, or only for specified exchanges."}, 
                                                                               &OptValueType::orderbook},
       {{{"Public queries", 2}, "--orderbook-depth", "<n>", "Override default depth of order book"}, &OptValueType::orderbook_depth},
       {{{"Public queries", 2}, "--orderbook-cur", "<cur>", "If conversion of cur2 into cur is possible (for each exchange), "
                                                            "prints additional column converted to given asset"}, 
                                                                 &OptValueType::orderbook_cur},
       {{{"Public queries", 2}, "--ticker", "[exch1,...]", "Print ticker information for all markets for all exchanges,"
                                                           " or only for specified ones"}, 
                                                           &OptValueType::ticker},
       {{{"Public queries", 2}, "--conversion", 'c', "<cur1-cur2[,exch1,...]>", "Print fastest conversion path of 'cur1' to 'cur2' "
                                                                                "for given exchanges if possible"}, 
                                                          &OptValueType::conversion_path},
       {{{"Public queries", 2}, "--volume-day", "<cur1-cur2[,exch1,...]>", "Print last 24h traded volume for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::last24hTradedVolume},
       {{{"Public queries", 2}, "--last-trades", "<cur1-cur2[,exch1,...]>", "Print last trades for market 'cur1'-'cur2' "
                                                                            "for all exchanges (or specified one)"}, 
                                                          &OptValueType::lastTrades},
       {{{"Public queries", 2}, "--last-trades-n", "<n>", string("Change number of last trades to query (default: ").
                                                            append(ToString<string>(api::ExchangePublic::kNbLastTradesDefault)).append(
                                                          ")")}, 
                                                          &OptValueType::nbLastTrades},  
       {{{"Public queries", 2}, "--price", 'p', "<cur1-cur2[,exch1,...]>", "Print last price for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::lastPrice},                                      

       {{{"Private queries", 3}, "--balance", 'b', "[exch1,...]", "Prints sum of available balance for all private accounts if no value is given, "
                                                                  "or only for specified ones separated by commas"}, 
                                                                  &OptValueType::balance},
       {{{"Private queries", 3}, "--balance-cur", "<cur code>", "Print additional information with each asset "
                                                                "converted to given currency, plus a total summary in this currency"}, 
                                                                &OptValueType::balance_cur},
       {{{"Private queries", 3}, "--orders-opened", "<cur1-cur2[,exch1,...]>", "Print opened orders with given selection criteria.\n"
                                                               "All cur1, cur2 and exchanges are optional,"
                                                               "returned opened orders will be filtered accordingly."}, 
                                                                &OptValueType::opened_orders_info},
       {{{"Private queries", 3}, "--orders-min-age", "<time>", "Only select orders with given minimum age.\n"}, 
                                                                &OptValueType::orders_min_age},
       {{{"Private queries", 3}, "--orders-max-age", "<time>", "Only select orders with given maximum age.\n"}, 
                                                                &OptValueType::orders_max_age},
       {{{"Trade", 4}, "--trade", 't', "<amt cur1-cur2[,exch1,...]>", "Single trade from given start amount on a list of exchanges, "
                                                                      "or all that have sufficient balance on cur1 if none provided.\n"
                                                                      "Order will be placed at limit price by default"}, &OptValueType::trade},
       {{{"Trade", 4}, "--trade-all", "<cur1-cur2[,exch1,...]>", "Single trade from available amount from given currency on a list of exchanges,"
                                                                 " or all that have some balance on cur1 if none provided\n"
                                                                 "Order will be placed at limit price by default"}, &OptValueType::trade_all},
       {{{"Trade", 4}, "--singletrade", "<amt cur1-cur2[,exch1,...]>", "Synonym for '--trade'"}, &OptValueType::trade},
       {{{"Trade", 4}, "--multitrade", "<amt cur1-cur2[,exch1,...]>", "Multi trade from given start amount on a list of exchanges,"
                                                                      " or all that have a sufficient balance on cur1 if none provided \n"
                                                                      "Multi trade will first compute fastest path from cur1 to cur2 and "
                                                                     "if possible reach cur2 by launching multiple single trades.\n"
                                                                     "Options are same than for single trade, applied to each step trade.\n"
                                                                     "If multi trade is used in conjonction with single trade, the latter is ignored."}, 
                                                                     &OptValueType::trade_multi},
       {{{"Trade", 4}, "--multitrade-all", "<cur1-cur2,exchange>", "Multi trade from available amount from given currency on an exchange.\n"
                                                                   "Order will be placed at limit price by default"}, &OptValueType::trade_multi_all},
       {{{"Trade", 4}, "--trade-strategy", "<maker|nibble|taker>", "Customize the order price strategy of the trade\n"
                                                                  " - 'maker': order price continuously set at limit price (default)\n"
                                                                  " - 'nibble': order price continuously set at limit price + (buy)/- (sell) 1\n"
                                                                  " - 'taker': order price will be at market price, expected to be matched directly"}, 
                                                                  &OptValueType::trade_price},
       {{{"Trade", 4}, "--trade-timeout", "<time>", string("Adjust trade timeout (default: ")
                                                .append(ToString<string>(defaultTradeTimeout))
                                                .append("s). Remaining orders will be cancelled after the timeout.")}, 
                                                &OptValueType::trade_timeout},
       {{{"Trade", 4}, "--trade-timeout-match", "", "If after the timeout some amount is still not traded,\n"
                                                    "force match by placing a remaining order at market price\n"}, 
                                                    &OptValueType::trade_timeout_match},
       {{{"Trade", 4}, "--trade-updateprice", "<time>", string("Set the min time allowed between two limit price updates (default: ")
                                                    .append(ToString<string>(minUpdatePriceTime))
                                                    .append("s). Avoids cancelling / placing new orders too often with high volumes "
                                                    "which can be counter productive sometimes.")}, &OptValueType::trade_updateprice},
       {{{"Trade", 4}, "--trade-sim", "", string("Activates simulation mode only (default: ")
                                         .append(isSimulationModeByDefault ? "true" : "false")
                                         .append("). For some exchanges, API can even be queried in this "
                                         "mode to ensure deeper and more realistic trading inputs")}, &OptValueType::trade_sim},
       {{{"Withdraw and deposit", 5}, "--deposit-info", "<cur[,exch1,...]>", "Get deposit wallet information for given currency."
                                                                             " If no exchange accounts are given, will query all of them by default"},
                                                                             &OptValueType::deposit_info},
       {{{"Withdraw and deposit", 5}, "--withdraw", 'w', "<amt cur,from-to>", string("Withdraw amount from exchange 'from' to exchange 'to'."
                                                                         " Amount is gross, including fees. Address and tag will be retrieved"
                                                                         " automatically from destination exchange and should match an entry in '")
                                                                        .append(kDepositAddressesFileName)
                                                                        .append("' file.")}, 
                                                                        &OptValueType::withdraw},
       {{{"Withdraw and deposit", 5}, "--withdraw-fee", "<cur[,exch1,...]>", string("Prints withdraw fees of given currency on all supported exchanges,"
                                                                         " or only for the list of specified ones if provided (comma separated).")}, 
                                                                &OptValueType::withdraw_fee},
       {{{"Monitoring", 6}, "--monitoring", "", "Progressively send metrics to external instance provided that it's correctly set up "
                                                "(Prometheus by default). Refer to the README for more information"}, 
                                                       &OptValueType::useMonitoring},
       {{{"Monitoring", 6}, "--monitoring-port", "<port>", string("Specify port of metric gateway instance (default: ")
                                                          .append(ToString<string>(CoincenterCmdLineOptions::kDefaultMonitoringPort)).append(")")}, 
                                                         &OptValueType::monitoring_port},
       {{{"Monitoring", 6}, "--monitoring-ip", "<IPv4>", string("Specify IP (v4) of metric gateway instance (default: ")
                                                        .append(CoincenterCmdLineOptions::kDefaultMonitoringIPAddress).append(")")}, 
                                                         &OptValueType::monitoring_address},
       {{{"Monitoring", 6}, "--monitoring-user", "<username>", "Specify username of metric gateway instance (default: none)"}, 
                                                                &OptValueType::monitoring_username},
       {{{"Monitoring", 6}, "--monitoring-pass", "<password>", "Specify password of metric gateway instance (default: none)"}, 
                                                                &OptValueType::monitoring_password}});
  // clang-format on
}
}  // namespace cct
