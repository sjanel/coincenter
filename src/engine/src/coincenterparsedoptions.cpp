#include "coincenterparsedoptions.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>

#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincenteroptions.hpp"
#include "commandlineoptionsparser.hpp"
#include "stringoptionparser.hpp"
#include "tradedefinitions.hpp"

namespace cct {

CoincenterParsedOptions::CoincenterParsedOptions(int argc, const char *argv[])
    : dataDir(kDefaultDataDir), _programName(std::filesystem::path(argv[0]).filename().string()) {
  using OptValueType = CoincenterCmdLineOptions;

  auto parser = CommandLineOptionsParser<OptValueType>(CoincenterAllowedOptions<OptValueType>::value);
  auto parsedOptions = parser.parse(argc, argv);

  if (parsedOptions.help || argc == 1) {
    parser.displayHelp(_programName, std::cout);
    noProcess = true;
  } else {
    setFromOptions(parsedOptions);
  }
}

namespace {
std::pair<OrdersConstraints, PrivateExchangeNames> ParseOrderRequest(const CoincenterCmdLineOptions &cmdLineOptions,
                                                                     std::string_view orderRequestStr) {
  auto currenciesPrivateExchangesTuple = StringOptionParser(orderRequestStr).getCurrenciesPrivateExchanges(false);
  auto orderIdViewVector = StringOptionParser(cmdLineOptions.ordersIds).getCSVValues();
  vector<OrderId> orderIds;
  orderIds.reserve(orderIdViewVector.size());
  for (std::string_view orderIdView : orderIdViewVector) {
    orderIds.emplace_back(orderIdView);
  }
  return std::make_pair(
      OrdersConstraints(std::get<0>(currenciesPrivateExchangesTuple), std::get<1>(currenciesPrivateExchangesTuple),
                        std::chrono::duration_cast<Duration>(cmdLineOptions.ordersMinAge),
                        std::chrono::duration_cast<Duration>(cmdLineOptions.ordersMaxAge),
                        OrdersConstraints::OrderIdSet(std::move(orderIds))),
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
  repeatTime = cmdLineOptions.repeatTime;
  printQueryResults = !cmdLineOptions.noPrint;

  monitoringInfo = MonitoringInfo(cmdLineOptions.useMonitoring, _programName, cmdLineOptions.monitoringAddress,
                                  cmdLineOptions.monitoringPort, cmdLineOptions.monitoringUsername,
                                  cmdLineOptions.monitoringPassword);

  if (!cmdLineOptions.markets.empty()) {
    StringOptionParser anyParser(cmdLineOptions.markets);
    std::tie(marketsCurrency1, marketsCurrency2, marketsExchanges) = anyParser.getCurrenciesPublicExchanges();
  }

  if (!cmdLineOptions.orderbook.empty()) {
    StringOptionParser anyParser(cmdLineOptions.orderbook);
    std::tie(marketForOrderBook, orderBookExchanges) = anyParser.getMarketExchanges();

    orderbookDepth = cmdLineOptions.orderbookDepth;
    orderbookCur = CurrencyCode(cmdLineOptions.orderbookCur);
  }

  if (cmdLineOptions.ticker) {
    StringOptionParser anyParser(*cmdLineOptions.ticker);
    tickerExchanges = anyParser.getExchanges();
    tickerForAll = tickerExchanges.empty();
  }

  if (!cmdLineOptions.conversionPath.empty()) {
    StringOptionParser anyParser(cmdLineOptions.conversionPath);
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
  bool isMultiTrade = !cmdLineOptions.tradeMulti.empty() || !cmdLineOptions.tradeMultiAll.empty();
  bool isTradeAll = !cmdLineOptions.tradeAll.empty() || !cmdLineOptions.tradeMultiAll.empty();
  if (isMultiTrade) {
    tradeArgs = isTradeAll ? cmdLineOptions.tradeMultiAll : cmdLineOptions.tradeMulti;
  } else {
    tradeArgs = isTradeAll ? cmdLineOptions.tradeAll : cmdLineOptions.trade;
  }
  if (!tradeArgs.empty()) {
    if (isTradeAll) {
      std::tie(fromTradeCurrency, toTradeCurrency, tradePrivateExchangeNames) =
          StringOptionParser(tradeArgs).getCurrenciesPrivateExchanges();
    } else {
      std::tie(startTradeAmount, isPercentageTrade, toTradeCurrency, tradePrivateExchangeNames) =
          StringOptionParser(tradeArgs).getMonetaryAmountCurrencyPrivateExchanges();
    }

    TradeMode tradeMode = cmdLineOptions.tradeSim ? TradeMode::kSimulation : TradeMode::kReal;
    TradeType tradeType = isMultiTrade ? TradeType::kMultiTradePossible : TradeType::kSingleTrade;
    TradeTimeoutAction timeoutAction =
        cmdLineOptions.tradeTimeoutMatch ? TradeTimeoutAction::kForceMatch : TradeTimeoutAction::kCancel;

    if (!cmdLineOptions.tradeStrategy.empty() && !cmdLineOptions.tradePrice.empty()) {
      throw invalid_argument("Trade price and trade strategy cannot be set together");
    }

    if (!cmdLineOptions.tradeStrategy.empty()) {
      PriceOptions priceOptions(cmdLineOptions.tradeStrategy);
      tradeOptions = TradeOptions(priceOptions, timeoutAction, tradeMode, cmdLineOptions.tradeTimeout,
                                  cmdLineOptions.tradeUpdatePrice, tradeType);
    } else if (!cmdLineOptions.tradePrice.empty()) {
      MonetaryAmount tradePrice(cmdLineOptions.tradePrice);
      if (tradePrice.isAmountInteger() && tradePrice.hasNeutralCurrency()) {
        // Then it must be a relative price
        RelativePrice relativePrice = static_cast<RelativePrice>(tradePrice.integerPart());
        PriceOptions priceOptions(relativePrice);
        tradeOptions = TradeOptions(priceOptions, timeoutAction, tradeMode, cmdLineOptions.tradeTimeout,
                                    cmdLineOptions.tradeUpdatePrice, tradeType);
      } else {
        PriceOptions priceOptions(tradePrice);
        tradeOptions = TradeOptions(priceOptions, timeoutAction, tradeMode, cmdLineOptions.tradeTimeout);
      }
    } else {
      tradeOptions = TradeOptions(timeoutAction, tradeMode, cmdLineOptions.tradeTimeout,
                                  cmdLineOptions.tradeUpdatePrice, tradeType);
    }
  }

  if (!cmdLineOptions.depositInfo.empty()) {
    StringOptionParser anyParser(cmdLineOptions.depositInfo);
    std::tie(depositCurrency, depositInfoPrivateExchanges) = anyParser.getCurrencyPrivateExchanges();
  }

  if (cmdLineOptions.openedOrdersInfo) {
    std::tie(openedOrdersConstraints, openedOrdersPrivateExchanges) =
        ParseOrderRequest(cmdLineOptions, *cmdLineOptions.openedOrdersInfo);
    queryOpenedOrders = true;
  }

  if (cmdLineOptions.cancelOpenedOrders) {
    std::tie(cancelOpenedOrdersConstraints, cancelOpenedOrdersPrivateExchanges) =
        ParseOrderRequest(cmdLineOptions, *cmdLineOptions.cancelOpenedOrders);
    cancelOpenedOrders = true;
  }

  if (!cmdLineOptions.withdraw.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw);
    std::tie(amountToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
        anyParser.getMonetaryAmountFromToPrivateExchange();
  }

  if (!cmdLineOptions.withdrawFee.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawFee);
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
