#pragma once

#include <chrono>
#include <string>
#include <utility>

#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "tradeoptionsapi.hpp"
#include "wallet.hpp"

namespace cct {

struct CoincenterCmdLineOptions {
  void setLogLevel() const;

  void setLogFile() const;

  static void PrintVersion(const char* programName);

  std::string logLevel{};
  bool help{};
  bool version{};
  bool logFile{};

  std::string orderbook{};
  int orderbook_depth{};
  std::string orderbook_cur{};

  std::string conversion_path{};

  std::string balance{};
  std::string balance_cur{CurrencyCode::kNeutral.str()};

  std::string trade{};
  std::string trade_strategy{api::TradeOptions().strategyStr()};
  int trade_timeout_s{static_cast<int>(
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultTradeDuration).count())};
  int trade_emergency_ms{static_cast<int>(
      std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultEmergencyTime).count())};
  int trade_updateprice_ms{static_cast<int>(
      std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultMinTimeBetweenPriceUpdates)
          .count())};
  bool trade_sim{api::TradeOptions().simulation()};

  std::string withdraw{};
};

template <class OptValueType>
inline CommandLineOptionsParser<OptValueType> CreateCoincenterCommandLineOptionsParser() {
  constexpr int64_t defaultTradeTimeout =
      std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultTradeDuration).count();
  constexpr int64_t emergencyBufferTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultEmergencyTime).count();
  constexpr int64_t minUpdatePriceTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultMinTimeBetweenPriceUpdates)
          .count();
  const bool isSimulationModeByDefault = api::TradeOptions().simulation();

  // clang-format off
  return CommandLineOptionsParser<OptValueType>(
      {{{{"General", 1}, "--help", 'h', "", "Display this information"}, &OptValueType::help},
       {{{"General", 1}, "--version", "", "Display program version"}, &OptValueType::version},
       {{{"General", 1}, "--loglevel", "<levelname>", "Sets the log level during all execution. "
                                                      "Possible values are: trace|debug|info|warning|error|critical|off"}, 
                                                      &OptValueType::logLevel},
       {{{"General", 1}, "--logfile", "", "Log to rotating files instead of stdout / stderr"}, &OptValueType::logFile},

       {{{"Public queries", 2}, "--orderbook", 'o', "<cur1-cur2,exch1,...>", "Print order book of currency pair for given exchanges"}, 
                                                                             &OptValueType::orderbook},
       {{{"Public queries", 2}, "--orderbook-depth", "", "Override default depth of order book"}, &OptValueType::orderbook_depth},
       {{{"Public queries", 2}, "--orderbook-cur", "<cur code>", "If conversion of cur2 into cur is possible on exch1, "
                                                                 "prints additional column converted to given asset"}, 
                                                                 &OptValueType::orderbook_cur},
       {{{"Public queries", 2}, "--conversion-path", 'c', "<cur1-cur2,exch1,...>", "Print fastest conversion path of 'cur1' to 'cur2' for given exchanges if possible"}, 
                                                                 &OptValueType::conversion_path},

       {{{"Private queries", 3}, "--balance", 'b', "<exch1,...|all>", "Prints available balance on given exchanges. "
                                                                      "If 'all' is specified, sums all available private exchange accounts"}, 
                                                                      &OptValueType::balance},
       {{{"Private queries", 3}, "--balance-cur", "<cur code>", "Print additional information with each asset "
                                                                "converted to given currency, plus a total summary"}, 
                                                                &OptValueType::balance_cur},

       {{{"Trade", 4}, "--trade", 't', "<amt cur1-cur2,exchange>", "Single trade from given start amount on an exchange. "
                                                                   "Order will be placed at limit price by default"}, &OptValueType::trade},
       {{{"Trade", 4}, "--trade-strategy", "<maker|taker|adapt>", "Customize the strategy of the trade\n"
                                                                  " - 'maker': order placed at limit price (default), continuously "
                                                                  "adjusted to limit price\n"
                                                                  " - 'taker': order placed at market price should be matched directly\n"
                                                                  " - 'adapt': same as maker, except that order will be updated"
                                                                  " at market price before the timeout to make it eventually completely matched"}, 
                                                                  &OptValueType::trade_strategy},
       {{{"Trade", 4}, "--trade-timeout", "<s>", std::string("Adjust trade timeout (default: ")
                                                .append(std::to_string(defaultTradeTimeout))
                                                .append("). Remaining orders will be cancelled after the timeout.")}, 
                                                &OptValueType::trade_timeout_s},
       {{{"Trade", 4}, "--trade-emergency", "<ms>", std::string("Adjust emergency buffer for the 'adapt' strategy (default: ")
                                                    .append(std::to_string(emergencyBufferTime))
                                                    .append("). Remaining order will be switched from limit to market price "
                                                    "after 'timeout - emergency' time to force completion of the trade")}, 
                                                    &OptValueType::trade_emergency_ms},
       {{{"Trade", 4}, "--trade-updateprice", "<ms>", std::string("Set the min time allowed between two limit price updates (default: ")
                                                     .append(std::to_string(minUpdatePriceTime))
                                                     .append("). Avoids cancelling / placing new orders too often with high volumes "
                                                     "which can be counter productive sometimes.")}, &OptValueType::trade_updateprice_ms},
       {{{"Trade", 4}, "--trade-sim", "", std::string("Activates simulation mode only (default: ")
                                         .append(isSimulationModeByDefault ? "true" : "false")
                                         .append("). For some exchanges, API can even be queried in this "
                                         "mode to ensure deeper and more realistic trading inputs")}, &OptValueType::trade_sim},

       {{{"Withdraw crypto", 5}, "--withdraw", 'w', "<amt cur,from-to>", std::string("Withdraw amount from exchange 'from' to exchange 'to'."
                                                                         " Amount is gross, including fees. Address and tag will be "
                                                                         "retrieved automatically from '")
                                                                        .append(Wallet::kDepositAddressesFilename)
                                                                        .append(".json' file. Make sure that values are up to date (and "
                                                                        "correct of course!)")}, &OptValueType::withdraw}});
  // clang-format on
}
}  // namespace cct