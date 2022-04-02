#include "coincentercommands.hpp"

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

CoincenterCmdLineOptions CoincenterCommands::parseOptions(int argc, const char *argv[]) const {
  using OptValueType = CoincenterCmdLineOptions;

  auto parser = CommandLineOptionsParser<OptValueType>(CoincenterAllowedOptions<OptValueType>::value);
  CoincenterCmdLineOptions parsedOptions = parser.parse(argc, argv);

  auto programName = std::filesystem::path(argv[0]).filename().string();
  if (parsedOptions.help) {
    parser.displayHelp(programName, std::cout);
  } else if (parsedOptions.version) {
    CoincenterCmdLineOptions::PrintVersion(programName);
  }
  return parsedOptions;
}

MonitoringInfo CoincenterCommands::createMonitoringInfo(std::string_view programName,
                                                        const CoincenterCmdLineOptions &cmdLineOptions) const {
  return MonitoringInfo(cmdLineOptions.useMonitoring, programName, cmdLineOptions.monitoringAddress,
                        cmdLineOptions.monitoringPort, cmdLineOptions.monitoringUsername,
                        cmdLineOptions.monitoringPassword);
}

namespace {
std::pair<OrdersConstraints, ExchangeNames> ParseOrderRequest(const CoincenterCmdLineOptions &cmdLineOptions,
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

bool CoincenterCommands::setFromOptions(const CoincenterCmdLineOptions &cmdLineOptions) {
  if (cmdLineOptions.help || cmdLineOptions.version) {
    return false;
  }

  if (cmdLineOptions.repeats.isPresent()) {
    if (cmdLineOptions.repeats.isSet()) {
      repeats = *cmdLineOptions.repeats;
    } else {
      // infinite repeats
      repeats = -1;
    }
  }

  repeatTime = cmdLineOptions.repeatTime;

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

  // Parse trade / buy / sell options
  // First, check that at most one master trade option is set
  // (options would be set for all trades otherwise which is not very intuitive)
  if (!cmdLineOptions.buy.empty() + !cmdLineOptions.sell.empty() + !cmdLineOptions.sellAll.empty() +
          !cmdLineOptions.tradeAll.empty() >
      1) {
    throw invalid_argument("Only one trade can be done at a time");
  }
  std::string_view tradeArgs;
  bool isSmartTrade = !cmdLineOptions.buy.empty() || !cmdLineOptions.sell.empty() || !cmdLineOptions.sellAll.empty();
  bool isTradeAll = !cmdLineOptions.tradeAll.empty();
  if (!cmdLineOptions.buy.empty()) {
    tradeArgs = cmdLineOptions.buy;
  } else if (!cmdLineOptions.sellAll.empty()) {
    tradeArgs = cmdLineOptions.sellAll;
  } else if (!cmdLineOptions.sell.empty()) {
    tradeArgs = cmdLineOptions.sell;
  } else {
    tradeArgs = isTradeAll ? cmdLineOptions.tradeAll : cmdLineOptions.trade;
  }
  if (!tradeArgs.empty()) {
    StringOptionParser optParser(tradeArgs);
    if (isSmartTrade) {
      if (!cmdLineOptions.sellAll.empty()) {
        std::tie(fromTradeCurrency, tradePrivateExchangeNames) = optParser.getCurrencyPrivateExchanges();
        startTradeAmount = MonetaryAmount(100, fromTradeCurrency);
        isPercentageTrade = true;
      } else {
        MonetaryAmount &specifiedAmount = !cmdLineOptions.buy.empty() ? endTradeAmount : startTradeAmount;
        std::tie(specifiedAmount, isPercentageTrade, tradePrivateExchangeNames) =
            optParser.getMonetaryAmountPrivateExchanges();
        if (specifiedAmount.isNegativeOrZero()) {
          throw invalid_argument("Start trade amount should be positive");
        }
      }
    } else if (isTradeAll) {
      std::tie(fromTradeCurrency, toTradeCurrency, tradePrivateExchangeNames) =
          optParser.getCurrenciesPrivateExchanges();
    } else {
      std::tie(startTradeAmount, isPercentageTrade, toTradeCurrency, tradePrivateExchangeNames) =
          optParser.getMonetaryAmountCurrencyPrivateExchanges();
      if (startTradeAmount.isNegativeOrZero()) {
        throw invalid_argument("Start trade amount should be positive");
      }
    }

    if (!cmdLineOptions.tradeStrategy.empty() && !cmdLineOptions.tradePrice.empty()) {
      throw invalid_argument("Trade price and trade strategy cannot be set together");
    }

    TradeMode tradeMode = cmdLineOptions.tradeSim ? TradeMode::kSimulation : TradeMode::kReal;
    TradeTimeoutAction timeoutAction =
        cmdLineOptions.tradeTimeoutMatch ? TradeTimeoutAction::kForceMatch : TradeTimeoutAction::kCancel;

    TradeTypePolicy tradeType = TradeTypePolicy::kDefault;
    if (cmdLineOptions.forceMultiTrade) {
      if (cmdLineOptions.forceSingleTrade) {
        throw invalid_argument("Multi & Single trade cannot be forced at the same time");
      }
      tradeType = TradeTypePolicy::kForceMultiTrade;
    } else if (cmdLineOptions.forceSingleTrade) {
      tradeType = TradeTypePolicy::kForceSingleTrade;
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
        if (isSmartTrade) {
          throw invalid_argument("Absolute price is not compatible with smart buy / sell");
        }
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
    std::tie(amountToWithdraw, isPercentageWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
        anyParser.getMonetaryAmountFromToPrivateExchange();
  }

  if (!cmdLineOptions.withdrawAll.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawAll);
    CurrencyCode curToWithdraw;
    std::tie(curToWithdraw, withdrawFromExchangeName, withdrawToExchangeName) =
        anyParser.getCurrencyFromToPrivateExchange();
    amountToWithdraw = MonetaryAmount(100, curToWithdraw);
    isPercentageWithdraw = true;
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

  return true;
}
}  // namespace cct
