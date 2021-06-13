#include "coincenterparsedoptions.hpp"

#include <chrono>
#include <iostream>

#include "cct_log.hpp"
#include "coincenteroptions.hpp"
#include "stringoptionparser.hpp"

namespace cct {
CoincenterParsedOptions::CoincenterParsedOptions(int argc, const char *argv[]) {
  try {
    CommandLineOptionsParser<CoincenterCmdLineOptions> cmdLineOptionsParser =
        CreateCoincenterCommandLineOptionsParser<CoincenterCmdLineOptions>();
    CoincenterCmdLineOptions cmdLineOptions = cmdLineOptionsParser.parse(argc, argv);
    if (cmdLineOptions.help || argc == 1) {
      cmdLineOptionsParser.displayHelp(argv[0], std::cout);
      noProcess = true;
    } else {
      setFromOptions(cmdLineOptions, argv[0]);
    }
  } catch (const InvalidArgumentException &e) {
    std::cerr << e.what() << std::endl;
    throw;
  }
}

void CoincenterParsedOptions::setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions, const char *programName) {
  if (cmdLineOptions.version) {
    CoincenterCmdLineOptions::PrintVersion(programName);
    noProcess = true;
    return;
  }

  log::set_level(log::level::info);

  cmdLineOptions.setLogLevel();
  cmdLineOptions.setLogFile();

  if (!cmdLineOptions.markets.empty()) {
    StringOptionParser anyParser(cmdLineOptions.markets);
    std::tie(marketsCurrency, marketsExchanges) = anyParser.getCurrencyCodePublicExchanges();
  }

  if (!cmdLineOptions.orderbook.empty()) {
    StringOptionParser anyParser(cmdLineOptions.orderbook);
    std::tie(marketForOrderBook, orderBookExchanges) = anyParser.getMarketExchanges();

    orderbookDepth = cmdLineOptions.orderbook_depth;
    orderbookCur = CurrencyCode(cmdLineOptions.orderbook_cur);
  }

  if (!cmdLineOptions.conversion_path.empty()) {
    StringOptionParser anyParser(cmdLineOptions.conversion_path);
    std::tie(marketForConversionPath, conversionPathExchanges) = anyParser.getMarketExchanges();
  }

  if (cmdLineOptions.balance) {
    StringOptionParser anyParser(*cmdLineOptions.balance);
    balancePrivateExchanges = anyParser.getPrivateExchanges();
    balanceForAll = balancePrivateExchanges.empty();
    balanceCurrencyCode = CurrencyCode(cmdLineOptions.balance_cur);
  }

  if (!cmdLineOptions.trade.empty()) {
    StringOptionParser anyParser(cmdLineOptions.trade);
    std::tie(startTradeAmount, toTradeCurrency, tradePrivateExchangeName) =
        anyParser.getMonetaryAmountCurrencyCodePrivateExchange();

    tradeOptions = api::TradeOptions(
        cmdLineOptions.trade_strategy, cmdLineOptions.trade_sim ? api::TradeMode::kSimulation : api::TradeMode::kReal,
        cmdLineOptions.trade_timeout, cmdLineOptions.trade_emergency, cmdLineOptions.trade_updateprice);
  }

  if (!cmdLineOptions.withdraw.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw);
    std::tie(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
        anyParser.getMonetaryAmountFromToPrivateExchange();
  }

  if (!cmdLineOptions.withdraw_fee.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw_fee);
    std::tie(withdrawFeeCur, withdrawFeeExchanges) = anyParser.getCurrencyCodePublicExchanges();
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    StringOptionParser anyParser(cmdLineOptions.last24hTradedVolume);
    std::tie(tradedVolumeMarket, tradedVolumeExchanges) = anyParser.getMarketExchanges();
  }
}
}  // namespace cct