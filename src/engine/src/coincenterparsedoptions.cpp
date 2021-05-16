#include "coincenterparsedoptions.hpp"

#include <chrono>
#include <iostream>

#include "cct_log.hpp"
#include "coincenteroptions.hpp"
#include "commandlineoptionsparser.hpp"
#include "stringoptionparser.hpp"

namespace cct {
CoincenterParsedOptions::CoincenterParsedOptions(int argc, const char *argv[]) {
  try {
    CommandLineOptionsParser<CoincenterCmdLineOptions> coincenterCommandLineOptionsParser =
        CreateCoincenterCommandLineOptionsParser<CoincenterCmdLineOptions>();
    CoincenterCmdLineOptions cmdLineOptions = coincenterCommandLineOptionsParser.parse(argc, argv);
    if (cmdLineOptions.help) {
      coincenterCommandLineOptionsParser.displayHelp(argv[0], std::cout);
      noProcess = true;
      return;
    }
    if (cmdLineOptions.version) {
      CoincenterCmdLineOptions::PrintVersion(argv[0]);
      noProcess = true;
      return;
    }
    log::set_level(log::level::info);

    cmdLineOptions.setLogLevel();
    cmdLineOptions.setLogFile();

    if (!cmdLineOptions.orderbook.empty()) {
      AnyParser anyParser(cmdLineOptions.orderbook);
      std::tie(marketForOrderBook, orderBookExchanges) = anyParser.getMarketExchanges();

      orderbookDepth = cmdLineOptions.orderbook_depth;
      orderbookCur = CurrencyCode(cmdLineOptions.orderbook_cur);
    }

    if (!cmdLineOptions.conversion_path.empty()) {
      AnyParser anyParser(cmdLineOptions.conversion_path);
      std::tie(marketForConversionPath, conversionPathExchanges) = anyParser.getMarketExchanges();
    }

    if (!cmdLineOptions.balance.empty()) {
      AnyParser anyParser(cmdLineOptions.balance);
      balancePrivateExchanges = anyParser.getPrivateExchanges();
      balanceCurrencyCode = CurrencyCode(cmdLineOptions.balance_cur);
    }

    if (!cmdLineOptions.trade.empty()) {
      AnyParser anyParser(cmdLineOptions.trade);
      std::tie(startTradeAmount, toTradeCurrency, tradePrivateExchangeName) =
          anyParser.getMonetaryAmountCurrencyCodePrivateExchange();

      tradeOptions = api::TradeOptions(cmdLineOptions.trade_strategy,
                                       cmdLineOptions.trade_sim ? api::TradeMode::kSimulation : api::TradeMode::kReal,
                                       std::chrono::seconds(cmdLineOptions.trade_timeout_s),
                                       std::chrono::milliseconds(cmdLineOptions.trade_emergency_ms),
                                       std::chrono::milliseconds(cmdLineOptions.trade_updateprice_ms));
    }

    if (!cmdLineOptions.withdraw.empty()) {
      AnyParser anyParser(cmdLineOptions.withdraw);
      std::tie(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
          anyParser.getMonetaryAmountFromToPrivateExchange();
    }

  } catch (const InvalidArgumentException &e) {
    std::cerr << e.what() << std::endl;
    throw;
  }
}
}  // namespace cct