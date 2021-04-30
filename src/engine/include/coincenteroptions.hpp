#pragma once

#include <chrono>
#include <string>
#include <utility>

#include "cct_smallvector.hpp"
#include "commandlineoptionsparser.hpp"
#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "tradeoptionsapi.hpp"

namespace cct {

struct CoincenterCmdLineOptions {
  void setLogLevel() const;

  void setLogFile() const;

  static void PrintUsage(const char* programName, const char* error = nullptr);

  static void PrintVersion(const char* programName);

  std::string logLevel{};
  bool help{};
  bool version{};
  bool logFile{};

  std::string orderbook{};
  int orderbook_depth{};
  std::string orderbook_cur{};

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

inline CoincenterCmdLineOptions ParseCoincenterCmdLineOptions(const vector<std::string_view>& vargv) {
  auto parser = CmdOpts<CoincenterCmdLineOptions>::Create(
      {{"--help", &CoincenterCmdLineOptions::help},
       {"--version", &CoincenterCmdLineOptions::version},
       {"--loglevel", &CoincenterCmdLineOptions::logLevel},
       {"--logfile", &CoincenterCmdLineOptions::logFile},

       {"--orderbook", &CoincenterCmdLineOptions::orderbook},
       {"--orderbook-depth", &CoincenterCmdLineOptions::orderbook_depth},
       {"--orderbook-cur", &CoincenterCmdLineOptions::orderbook_cur},

       {"--balance", &CoincenterCmdLineOptions::balance},
       {"--balance-cur", &CoincenterCmdLineOptions::balance_cur},

       {"--trade", &CoincenterCmdLineOptions::trade},
       {"--trade-strategy", &CoincenterCmdLineOptions::trade_strategy},
       {"--trade-timeout", &CoincenterCmdLineOptions::trade_timeout_s},
       {"--trade-emergency", &CoincenterCmdLineOptions::trade_emergency_ms},
       {"--trade-updateprice", &CoincenterCmdLineOptions::trade_updateprice_ms},
       {"--trade-sim", &CoincenterCmdLineOptions::trade_sim},

       {"--withdraw", &CoincenterCmdLineOptions::withdraw}});
  return parser.parse(vargv);
}

}  // namespace cct