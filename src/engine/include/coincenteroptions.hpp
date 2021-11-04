#pragma once

#include <chrono>
#include <optional>
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

  void setLogLevel() const;

  void setLogFile() const;

  static void PrintVersion(const char* programName);

  string dataDir = kDefaultDataDir;

  string logLevel;
  bool help = false;
  bool version = false;
  bool logFile = false;
  std::optional<string> nosecrets;

  string markets;

  string orderbook;
  int orderbook_depth{};
  string orderbook_cur;

  string conversion_path;

  std::optional<string> balance;
  string balance_cur{CurrencyCode::kNeutral.str()};

  string trade;
  string trade_strategy{api::TradeOptions().strategyStr()};
  Duration trade_timeout{api::TradeOptions::kDefaultTradeDuration};
  Duration trade_emergency{api::TradeOptions::kDefaultEmergencyTime};
  Duration trade_updateprice{api::TradeOptions::kDefaultMinTimeBetweenPriceUpdates};
  bool trade_sim{api::TradeOptions().isSimulation()};

  string withdraw;
  string withdraw_fee;

  string last24hTradedVolume;
  string lastPrice;
};

template <class OptValueType>
CommandLineOptionsParser<OptValueType> CreateCoincenterCommandLineOptionsParser() {
  constexpr int64_t defaultTradeTimeout =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultTradeDuration).count();
  constexpr int64_t emergencyBufferTime =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultEmergencyTime).count();
  constexpr int64_t minUpdatePriceTime =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultMinTimeBetweenPriceUpdates).count();
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

       {{{"Trade", 4}, "--trade", 't', "<amt cur1-cur2,exchange>", "Single trade from given start amount on an exchange "
                                                                   "Order will be placed at limit price by default"}, &OptValueType::trade},
       {{{"Trade", 4}, "--trade-strategy", "<maker|taker|adapt>", "Customize the strategy of the trade\n"
                                                                  " - 'maker': order placed at limit price (default), continuously "
                                                                  "adjusted to limit price\n"
                                                                  " - 'taker': order placed at market price should be matched directly\n"
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
