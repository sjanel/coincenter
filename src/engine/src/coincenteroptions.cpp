#include "coincenteroptions.hpp"

#include <spdlog/sinks/rotating_file_sink.h>

#include <iostream>

#include "cct_const.hpp"
#include "cct_log.hpp"
#include "stringoptionparser.hpp"
#include "tradeoptionsapi.hpp"
#include "wallet.hpp"

namespace cct {

void CoincenterCmdLineOptions::PrintUsage(const char* programName, const char* error) {
  if (error) {
    std::cerr << error << std::endl;
  }
  std::cout << "usage: " << programName << " [options]" << std::endl;
  std::cout << "Options:" << std::endl;

  std::cout << " General" << std::endl;
  std::cout << "  --help                    Display this information." << std::endl;
  std::cout << "  --version                 Display trade program version." << std::endl;
  std::cout << "  --loglevel <levelname>    Sets the log level during all execution." << std::endl;
  std::cout << "                            Possible values are: trace|debug|info|warning|error|critical|off"
            << std::endl;
  std::cout << "  --logfile                 Log to rotating files instead of stdout / stderr." << std::endl;

  std::cout << std::endl;
  // clang-format off
  std::cout << " Public queries" << std::endl;
  std::cout << std::endl;
  std::cout << "  --orderbook <cur1-cur2,exch1,...>  Print order book of currency pair on a list of exchanges." << std::endl;
  std::cout << "  --orderbook-depth <d>              Override default depth of order book" << std::endl;
  std::cout << "  --orderbook-cur <cur code>         If conversion of cur2 into cur is possible on exch1," << std::endl;
  std::cout << "                                     prints additional column converted to given asset" << std::endl;
  std::cout << std::endl;

  std::cout << " Private queries" << std::endl;
  std::cout << std::endl;
  std::cout << "  --balance <exch1,...|all>          Prints available balance on given exchanges" << std::endl;
  std::cout << "                                     If 'all' is specified, sums all available private exchange accounts" << std::endl;
  std::cout << "  --balance-cur <cur code>           Print additional information with each asset" << std::endl;
  std::cout << "                                     converted to given currency, plus a total summary" << std::endl;
  std::cout << std::endl;

  std::cout << " Trade" << std::endl;
  std::cout << std::endl;
  std::cout << "  --trade <amt cur1-cur2,exchange>   Single trade from given start amount on an exchange" << std::endl;
  std::cout << "                                     Order will be placed at limit price by default" << std::endl;
  std::cout << "  --trade-strategy <strategy>        Customize the strategy of the trade." << std::endl;
  std::cout << "                                     Possible values are: maker|taker|adapt" << std::endl;
  std::cout << "                                      - maker: order placed at limit price (default)" << std::endl;
  std::cout << "                                               price is continuously adjusted to limit price" << std::endl;
  std::cout << "                                      - taker: order placed at market price" << std::endl;
  std::cout << "                                               should be matched directly" << std::endl;
  std::cout << "                                      - adapt: same as maker, except that order will be " << std::endl;
  std::cout << "                                               updated at market price before the timeout" << std::endl;
  std::cout << "                                               to make it eventually completely matched" << std::endl;
  api::TradeOptions tradeOptionsDefault;
  constexpr int64_t defaultTradeTimeout = std::chrono::duration_cast<std::chrono::seconds>(api::TradeOptions::kDefaultTradeDuration).count();
  std::cout << "  --trade-timeout <s>                Adjust trade timeout (default: " << defaultTradeTimeout << ")" << std::endl;
  std::cout << "                                     Remaining orders will be cancelled after the timeout." << std::endl;
  constexpr int64_t emergencyBufferTime = std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultEmergencyTime).count();
  std::cout << "  --trade-emergency <ms>             Adjust emergency buffer for the 'adapt' strategy (default: " << emergencyBufferTime << ")" << std::endl;
  std::cout << "                                     Remaining order will be switched from limit to market price" << std::endl;
  std::cout << "                                     after 'timeout - emergency' time to force completion of the trade" << std::endl;
  constexpr int64_t minUpdatePriceTime = std::chrono::duration_cast<std::chrono::milliseconds>(api::TradeOptions::kDefaultMinTimeBetweenPriceUpdates).count();
  std::cout << "  --trade-updateprice <ms>           Set the min time allowed between two limit price updates (default: " << minUpdatePriceTime << ")" << std::endl;
  std::cout << "                                     Avoids cancelling / placing new orders too often with high volumes," << std::endl;
  std::cout << "                                     which can be counter productive sometimes." << std::endl;
  const bool isSimulationModeByDefault = tradeOptionsDefault.simulation();
  std::cout << "  --trade-sim                        Activates simulation mode only (default: " << (isSimulationModeByDefault ? "true" : "false") << ")" << std::endl;
  std::cout << "                                     For some exchanges (Kraken) API can even be queried in this mode" << std::endl;
  std::cout << "                                     to ensure deeper and more realistic trading inputs." << std::endl;
  std::cout << std::endl;
  std::cout << " Withdraw crypto" << std::endl;
  std::cout << std::endl;
  std::cout << "  --withdraw <amt cur,from-to>       Withdraw amount from exchange 'from' to exchange 'to'" << std::endl;
  std::cout << "                                     Amount is gross, including fees. Address and tag will be retrieved " << std::endl;
  std::cout << "                                     automatically from '" << Wallet::kDepositAddressesFilename << ".json' file. Make sure" << std::endl;
  std::cout << "                                     that values are up to date (and correct of course!). " << std::endl;
  // clang-format on
  std::cout << std::endl;
}

void CoincenterCmdLineOptions::PrintVersion(const char* programName) {
  std::cout << programName << " version " << kVersion << std::endl;
  std::cout << "compiled with " << CCT_COMPILER_VERSION << " on " << __DATE__ << " at " << __TIME__ << std::endl;
}

void CoincenterCmdLineOptions::setLogLevel() const {
  if (logLevel.empty()) {
    log::set_level(log::level::info);
  } else {
    log::set_level(log::level::from_str(logLevel));
  }
}

void CoincenterCmdLineOptions::setLogFile() const {
  if (logFile) {
    constexpr int max_size = 1048576 * 5;
    constexpr int max_files = 10;
    log::set_default_logger(log::rotating_logger_st("main", "log/log.txt", max_size, max_files));
  }
}

}  // namespace cct