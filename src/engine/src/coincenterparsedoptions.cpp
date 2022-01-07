#include "coincenterparsedoptions.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

#include "cct_log.hpp"
#include "coincenteroptions.hpp"
#include "stringoptionparser.hpp"
#include "tradedefinitions.hpp"

namespace cct {
CoincenterParsedOptions::CoincenterParsedOptions(int argc, const char *argv[])
    : dataDir(kDefaultDataDir), _programName(std::filesystem::path(argv[0]).filename().string()) {
  try {
    CommandLineOptionsParser<CoincenterCmdLineOptions> cmdLineOptionsParser =
        CreateCoincenterCommandLineOptionsParser<CoincenterCmdLineOptions>();
    CoincenterCmdLineOptions cmdLineOptions = cmdLineOptionsParser.parse(argc, argv);

    if (cmdLineOptions.help || argc == 1) {
      cmdLineOptionsParser.displayHelp(_programName, std::cout);
      noProcess = true;
    } else {
      setFromOptions(cmdLineOptions);
    }
  } catch (const InvalidArgumentException &e) {
    std::cerr << e.what() << std::endl;
    throw;
  }
}

namespace {
std::pair<OrdersConstraints, PrivateExchangeNames> ParseOrderRequest(const CoincenterCmdLineOptions &cmdLineOptions,
                                                                     std::string_view orderRequestStr) {
  auto currenciesPrivateExchangesTuple = StringOptionParser(orderRequestStr).getCurrenciesPrivateExchanges(false);
  return std::make_pair(
      OrdersConstraints(std::get<0>(currenciesPrivateExchangesTuple), std::get<1>(currenciesPrivateExchangesTuple),
                        std::chrono::duration_cast<OrdersConstraints::Duration>(cmdLineOptions.orders_min_age),
                        std::chrono::duration_cast<OrdersConstraints::Duration>(cmdLineOptions.orders_max_age),
                        OrdersConstraints::OrderIdSet(StringOptionParser(cmdLineOptions.orders_ids).getCSVValues())),
      std::get<2>(currenciesPrivateExchangesTuple));
}
}  // namespace

void CoincenterParsedOptions::setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions) {
  if (cmdLineOptions.version) {
    CoincenterCmdLineOptions::PrintVersion(_programName);
    noProcess = true;
    return;
  }

  cmdLineOptions.setLogLevel();
  cmdLineOptions.setLogFile();

  dataDir = cmdLineOptions.dataDir;
  if (cmdLineOptions.repeats.isPresent()) {
    if (cmdLineOptions.repeats.isSet()) {
      repeats = *cmdLineOptions.repeats;
    } else {
      // infinite repeats
      repeats = -1;
    }
  }
  repeat_time = cmdLineOptions.repeat_time;
  printQueryResults = !cmdLineOptions.noPrint;

  if (cmdLineOptions.useMonitoring) {
    useMonitoring = true;
    monitoring_username = std::move(cmdLineOptions.monitoring_username);
    monitoring_address = std::move(cmdLineOptions.monitoring_address);
    monitoring_password = std::move(cmdLineOptions.monitoring_password);
    if (cmdLineOptions.monitoring_port < 0 ||
        cmdLineOptions.monitoring_port > static_cast<int>(std::numeric_limits<uint16_t>::max())) {
      throw InvalidArgumentException("Invalid port value");
    }
    monitoring_port = static_cast<uint16_t>(cmdLineOptions.monitoring_port);
  }

  if (!cmdLineOptions.markets.empty()) {
    StringOptionParser anyParser(cmdLineOptions.markets);
    std::tie(marketsCurrency1, marketsCurrency2, marketsExchanges) = anyParser.getCurrenciesPublicExchanges();
  }

  if (!cmdLineOptions.orderbook.empty()) {
    StringOptionParser anyParser(cmdLineOptions.orderbook);
    std::tie(marketForOrderBook, orderBookExchanges) = anyParser.getMarketExchanges();

    orderbookDepth = cmdLineOptions.orderbook_depth;
    orderbookCur = CurrencyCode(cmdLineOptions.orderbook_cur);
  }

  if (cmdLineOptions.ticker) {
    StringOptionParser anyParser(*cmdLineOptions.ticker);
    tickerExchanges = anyParser.getExchanges();
    tickerForAll = tickerExchanges.empty();
  }

  if (!cmdLineOptions.conversion_path.empty()) {
    StringOptionParser anyParser(cmdLineOptions.conversion_path);
    std::tie(marketForConversionPath, conversionPathExchanges) = anyParser.getMarketExchanges();
  }

  if (cmdLineOptions.balance) {
    StringOptionParser anyParser(*cmdLineOptions.balance);
    std::tie(balanceCurrencyCode, balancePrivateExchanges) = anyParser.getCurrencyPrivateExchanges();
    if (balancePrivateExchanges.empty()) {
      balanceForAll = true;
    }
  }

  if (cmdLineOptions.nosecrets) {
    StringOptionParser anyParser(*cmdLineOptions.nosecrets);

    exchangesSecretsInfo = ExchangeSecretsInfo(anyParser.getExchanges());
  }

  std::string_view tradeArgs;
  bool isMultiTrade = !cmdLineOptions.trade_multi.empty() || !cmdLineOptions.trade_multi_all.empty();
  bool isTradeAll = !cmdLineOptions.trade_all.empty() || !cmdLineOptions.trade_multi_all.empty();
  if (isMultiTrade) {
    tradeArgs = isTradeAll ? cmdLineOptions.trade_multi_all : cmdLineOptions.trade_multi;
  } else {
    tradeArgs = isTradeAll ? cmdLineOptions.trade_all : cmdLineOptions.trade;
  }
  if (!tradeArgs.empty()) {
    StringOptionParser anyParser(tradeArgs);
    if (isTradeAll) {
      std::tie(fromTradeCurrency, toTradeCurrency, tradePrivateExchangeNames) =
          anyParser.getCurrenciesPrivateExchanges();
    } else {
      std::tie(startTradeAmount, toTradeCurrency, tradePrivateExchangeNames) =
          anyParser.getMonetaryAmountCurrencyPrivateExchanges();
    }

    TradeMode tradeMode = cmdLineOptions.trade_sim ? TradeMode::kSimulation : TradeMode::kReal;
    TradeType tradeType = isMultiTrade ? TradeType::kMultiTradePossible : TradeType::kSingleTrade;
    TradeTimeoutAction timeoutAction =
        cmdLineOptions.trade_timeout_match ? TradeTimeoutAction::kForceMatch : TradeTimeoutAction::kCancel;

    tradeOptions = TradeOptions(cmdLineOptions.trade_price, timeoutAction, tradeMode, cmdLineOptions.trade_timeout,
                                cmdLineOptions.trade_updateprice, tradeType);
  }

  if (!cmdLineOptions.deposit_info.empty()) {
    StringOptionParser anyParser(cmdLineOptions.deposit_info);
    std::tie(depositCurrency, depositInfoPrivateExchanges) = anyParser.getCurrencyPrivateExchanges();
  }

  if (cmdLineOptions.opened_orders_info) {
    std::tie(openedOrdersConstraints, openedOrdersPrivateExchanges) =
        ParseOrderRequest(cmdLineOptions, *cmdLineOptions.opened_orders_info);
    queryOpenedOrders = true;
  }

  if (cmdLineOptions.cancel_opened_orders) {
    std::tie(cancelOpenedOrdersConstraints, cancelOpenedOrdersPrivateExchanges) =
        ParseOrderRequest(cmdLineOptions, *cmdLineOptions.cancel_opened_orders);
    cancelOpenedOrders = true;
  }

  if (!cmdLineOptions.withdraw.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw);
    std::tie(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
        anyParser.getMonetaryAmountFromToPrivateExchange();
  }

  if (!cmdLineOptions.withdraw_fee.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw_fee);
    std::tie(withdrawFeeCur, withdrawFeeExchanges) = anyParser.getCurrencyPublicExchanges();
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    StringOptionParser anyParser(cmdLineOptions.last24hTradedVolume);
    std::tie(tradedVolumeMarket, tradedVolumeExchanges) = anyParser.getMarketExchanges();
  }

  if (!cmdLineOptions.lastTrades.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastTrades);
    std::tie(lastTradesMarket, lastTradesExchanges) = anyParser.getMarketExchanges();
  }
  nbLastTrades = cmdLineOptions.nbLastTrades;

  if (!cmdLineOptions.lastPrice.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastPrice);
    std::tie(lastPriceMarket, lastPriceExchanges) = anyParser.getMarketExchanges();
  }
}
}  // namespace cct