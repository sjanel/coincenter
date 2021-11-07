#pragma once

#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "cct_const.hpp"
#include "cct_string.hpp"
#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "tradeoptions.hpp"
#include "wallet.hpp"

namespace cct {

struct CoincenterCmdLineOptions {
  using Duration = api::TradeOptions::Clock::duration;

  static constexpr std::string_view kDefaultMonitoringIPAddress = "0.0.0.0";  // in Docker, localhost does not work
  static constexpr int kDefaultMonitoringPort = 9090;                         // Prometheus default port

  void setLogLevel() const;

  void setLogFile() const;

  static void PrintVersion(std::string_view programName);

  string dataDir = kDefaultDataDir;

  string logLevel;
  bool help = false;
  bool version = false;
  bool logFile = false;
  std::optional<string> nosecrets;

  string monitoring_address = string(kDefaultMonitoringIPAddress);
  string monitoring_username;
  string monitoring_password;
  int monitoring_port = kDefaultMonitoringPort;
  bool useMonitoring = false;

  string markets;

  string orderbook;
  int orderbook_depth{};
  string orderbook_cur;

  string conversion_path;

  std::optional<string> balance;
  string balance_cur{CurrencyCode::kNeutral.str()};

  string trade;
  string trade_multi;
  string trade_strategy{api::TradeOptions().strategyStr()};
  Duration trade_timeout{api::TradeOptions().maxTradeTime()};
  Duration trade_emergency{api::TradeOptions().emergencyBufferTime()};
  Duration trade_updateprice{api::TradeOptions().minTimeBetweenPriceUpdates()};
  bool trade_sim{api::TradeOptions().isSimulation()};

  string withdraw;
  string withdraw_fee;

  string last24hTradedVolume;
  string lastPrice;
};

template <class OptValueType>
CommandLineOptionsParser<OptValueType> CreateCoincenterCommandLineOptionsParser() {
  constexpr int64_t defaultTradeTimeout =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions().maxTradeTime()).count();
  constexpr int64_t emergencyBufferTime =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions().emergencyBufferTime()).count();
  constexpr int64_t minUpdatePriceTime =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions().minTimeBetweenPriceUpdates()).count();
  constexpr bool isSimulationModeByDefault = api::TradeOptions().isSimulation();

  // clang-format off
  return CommandLineOptionsParser<OptValueType>(
      {{{{"General", 1}, "--help", 'h', "", "Display this information"}, &OptValueType::help},
       {{{"General", 1}, "--version", "", "Display program version"}, &OptValueType::version},
       {{{"General", 1}, "--data", 'd', "<path/to/data>", string("Use given 'data' directory instead of the one chosen at build time '")
                                                      .append(kDefaultDataDir).append("'")}, 
                                                      &OptValueType::dataDir},
       {{{"General", 1}, "--loglevel", 'v', "<levelname|0-6>", string("Sets the log level during all execution. "
                                                      "Possible values are: \n(off|critical|error|warning|info|debug|trace) or "
                                                      "(0-6) (default: ").append(log::level::to_string_view(log::get_level()).data()).append(")")}, 
                                                      &OptValueType::logLevel},
       {{{"General", 1}, "--logfile", "", "Log to rotating files instead of stdout / stderr"}, &OptValueType::logFile},
       {{{"General", 1}, "--nosecrets", "[exch1,...]", "Even if present, do not load secrets and do not use private exchanges.\n"
                                                       "If empty list of exchanges, it skips secrets load for all private exchanges"}, 
                                                       &OptValueType::nosecrets},
       {{{"Monitoring", 1}, "--monitoring", "", "Progressively send metrics to external instance provided that it's correctly set up "
                                                "(Prometheus by default). Refer to the README for more information"}, 
                                                       &OptValueType::useMonitoring},
       {{{"Monitoring", 1}, "--monitoring-port", "<port>", string("Specify port of metric gateway instance (default: ")
                                                          .append(std::to_string(CoincenterCmdLineOptions::kDefaultMonitoringPort)).append(")")}, 
                                                         &OptValueType::monitoring_port},
       {{{"Monitoring", 1}, "--monitoring-ip", "<IPv4>", string("Specify IP (v4) of metric gateway instance (default: ")
                                                        .append(CoincenterCmdLineOptions::kDefaultMonitoringIPAddress).append(")")}, 
                                                         &OptValueType::monitoring_address},
       {{{"Monitoring", 1}, "--monitoring-user", "<username>", "Specify username of metric gateway instance (default: none)"}, 
                                                            &OptValueType::monitoring_username},
       {{{"Monitoring", 1}, "--monitoring-pass", "<password>", "Specify password of metric gateway instance (default: none)"}, 
                                                            &OptValueType::monitoring_password},
       {{{"Public queries", 2}, "--markets", 'm', "<cur[,exch1,...]>", "Print markets involving given currency for all exchanges, or only the specified ones."}, 
                                                                       &OptValueType::markets},

       {{{"Public queries", 2}, "--orderbook", 'o', "<cur1-cur2[,exch1,...]>", "Print order book of currency pair for all exchanges offering "
                                                                               "this market, or only for specified exchanges."}, 
                                                                               &OptValueType::orderbook},
       {{{"Public queries", 2}, "--orderbook-depth", "<n>", "Override default depth of order book"}, &OptValueType::orderbook_depth},
       {{{"Public queries", 2}, "--orderbook-cur", "<cur>", "If conversion of cur2 into cur is possible (for each exchange), "
                                                            "prints additional column converted to given asset"}, 
                                                                 &OptValueType::orderbook_cur},
       {{{"Public queries", 2}, "--conversion-path", 'c', "<cur1-cur2[,exch1,...]>", "Print fastest conversion path of 'cur1' to 'cur2' "
                                                                                     "for given exchanges if possible"}, 
                                                          &OptValueType::conversion_path},
       {{{"Public queries", 2}, "--volume-day", "<cur1-cur2[,exch1,...]>", "Print last 24h traded volume for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::last24hTradedVolume},  
       {{{"Public queries", 2}, "--price", 'p', "<cur1-cur2[,exch1,...]>", "Print last price for market 'cur1'-'cur2' "
                                                                           "for all exchanges (or specified one)"}, 
                                                          &OptValueType::lastPrice},                                      

       {{{"Private queries", 3}, "--balance", 'b', "[exch1,...]", "Prints sum of available balance for all private accounts if no value is given, "
                                                                  "or only for specified ones separated by commas"}, 
                                                                  &OptValueType::balance},
       {{{"Private queries", 3}, "--balance-cur", "<cur code>", "Print additional information with each asset "
                                                                "converted to given currency, plus a total summary in this currency"}, 
                                                                &OptValueType::balance_cur},

       {{{"Trade", 4}, "--trade", 't', "<amt cur1-cur2,exchange>", "Single trade from given start amount on an exchange.\n"
                                                                   "Order will be placed at limit price by default"}, &OptValueType::trade},
       {{{"Trade", 4}, "--singletrade", "<amt cur1-cur2,exchange>", "Synonym for '--trade'"}, &OptValueType::trade},
       {{{"Trade", 4}, "--multitrade", "<amt cur1-cur2,exchange>", "Multi trade from given start amount on an exchange.\n"
                                                                   "Multi trade will first compute fastest path from cur1 to cur2 and "
                                                                   "if possible reach cur2 by launching multiple single trades.\n"
                                                                   "Options are same than for single trade, applied to each step trade.\n"
                                                                   "If multi trade is used in conjonction with single trade, the latter is ignored."}, 
                                                                   &OptValueType::trade_multi},
       {{{"Trade", 4}, "--trade-strategy", "<maker|taker|adapt>", "Customize the strategy of the trade\n"
                                                                  " - 'maker': order placed at limit price (default), continuously "
                                                                  "adjusted to limit price\n"
                                                                  " - 'taker': order placed at market price (should be matched directly)\n"
                                                                  " - 'adapt': same as maker, except that order will be updated"
                                                                  " at market price before the timeout to make it eventually completely matched. "
                                                                  "Useful for exchanges proposing cheaper maker than taker fees."}, 
                                                                  &OptValueType::trade_strategy},
       {{{"Trade", 4}, "--trade-timeout", "<time>", string("Adjust trade timeout (default: ")
                                                .append(std::to_string(defaultTradeTimeout))
                                                .append("s). Remaining orders will be cancelled after the timeout.")}, 
                                                &OptValueType::trade_timeout},
       {{{"Trade", 4}, "--trade-emergency", "<time>", string("Adjust emergency buffer for the 'adapt' strategy (default: ")
                                                   .append(std::to_string(emergencyBufferTime))
                                                   .append("s). Remaining order will be switched from limit to market price "
                                                   "after 'timeout - emergency' time to force completion of the trade")}, 
                                                   &OptValueType::trade_emergency},
       {{{"Trade", 4}, "--trade-updateprice", "<time>", string("Set the min time allowed between two limit price updates (default: ")
                                                    .append(std::to_string(minUpdatePriceTime))
                                                    .append("s). Avoids cancelling / placing new orders too often with high volumes "
                                                    "which can be counter productive sometimes.")}, &OptValueType::trade_updateprice},
       {{{"Trade", 4}, "--trade-sim", "", string("Activates simulation mode only (default: ")
                                         .append(isSimulationModeByDefault ? "true" : "false")
                                         .append("). For some exchanges, API can even be queried in this "
                                         "mode to ensure deeper and more realistic trading inputs")}, &OptValueType::trade_sim},

       {{{"Withdraw crypto", 5}, "--withdraw", 'w', "<amt cur,from-to>", string("Withdraw amount from exchange 'from' to exchange 'to'."
                                                                         " Amount is gross, including fees. Address and tag will be retrieved"
                                                                         " automatically from destination exchange and should match an entry in '")
                                                                        .append(kDepositAddressesFileName)
                                                                        .append("' file.")}, &OptValueType::withdraw},
       {{{"Withdraw crypto", 5}, "--withdraw-fee", "<cur[,exch1,...]>", string("Prints withdraw fees of given currency on all supported exchanges,"
                                                                         " or only for the list of specified ones if provided (comma separated).")}, 
                                                                &OptValueType::withdraw_fee}});
  // clang-format on
}
}  // namespace cct
