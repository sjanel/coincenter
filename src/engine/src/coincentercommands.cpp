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
      _repeats = *cmdLineOptions.repeats;
    } else {
      // infinite repeats
      _repeats = -1;
    }
  }

  _repeatTime = cmdLineOptions.repeatTime;

  if (!cmdLineOptions.markets.empty()) {
    StringOptionParser anyParser(cmdLineOptions.markets);
    auto [cur1, cur2, exchanges] = anyParser.getCurrenciesPublicExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kMarkets)
        .setCur1(cur1)
        .setCur2(cur2)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.orderbook.empty()) {
    StringOptionParser anyParser(cmdLineOptions.orderbook);
    auto [market, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kOrderbook)
        .setMarket(market)
        .setExchangeNames(std::move(exchanges))
        .setDepth(cmdLineOptions.orderbookDepth)
        .setCur1(cmdLineOptions.orderbookCur);
  }

  if (cmdLineOptions.ticker) {
    StringOptionParser anyParser(*cmdLineOptions.ticker);
    _commands.emplace_back(CoincenterCommand::Type::kTicker).setExchangeNames(anyParser.getExchanges());
  }

  if (!cmdLineOptions.conversionPath.empty()) {
    StringOptionParser anyParser(cmdLineOptions.conversionPath);
    auto [market, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kConversionPath)
        .setMarket(market)
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.balance) {
    StringOptionParser anyParser(*cmdLineOptions.balance);
    auto [balanceCurrencyCode, exchanges] = anyParser.getCurrencyPrivateExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kBalance)
        .setCur1(balanceCurrencyCode)
        .setExchangeNames(std::move(exchanges));
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
  CoincenterCommand::Type commandType;
  if (!cmdLineOptions.buy.empty()) {
    tradeArgs = cmdLineOptions.buy;
    commandType = CoincenterCommand::Type::kBuy;
  } else if (!cmdLineOptions.sellAll.empty()) {
    tradeArgs = cmdLineOptions.sellAll;
    commandType = CoincenterCommand::Type::kSell;
  } else if (!cmdLineOptions.sell.empty()) {
    tradeArgs = cmdLineOptions.sell;
    commandType = CoincenterCommand::Type::kSell;
  } else {
    tradeArgs = isTradeAll ? cmdLineOptions.tradeAll : cmdLineOptions.trade;
    commandType = CoincenterCommand::Type::kTrade;
  }
  if (!tradeArgs.empty()) {
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

    CoincenterCommand &coincenterCommand = _commands.emplace_back(commandType);

    if (!cmdLineOptions.tradeStrategy.empty()) {
      PriceOptions priceOptions(cmdLineOptions.tradeStrategy);
      coincenterCommand.setTradeOptions(TradeOptions(priceOptions, timeoutAction, tradeMode,
                                                     cmdLineOptions.tradeTimeout, cmdLineOptions.tradeUpdatePrice,
                                                     tradeType));
    } else if (!cmdLineOptions.tradePrice.empty()) {
      MonetaryAmount tradePrice(cmdLineOptions.tradePrice);
      if (tradePrice.isAmountInteger() && tradePrice.hasNeutralCurrency()) {
        // Then it must be a relative price
        RelativePrice relativePrice = static_cast<RelativePrice>(tradePrice.integerPart());
        PriceOptions priceOptions(relativePrice);
        coincenterCommand.setTradeOptions(TradeOptions(priceOptions, timeoutAction, tradeMode,
                                                       cmdLineOptions.tradeTimeout, cmdLineOptions.tradeUpdatePrice,
                                                       tradeType));
      } else {
        if (isSmartTrade) {
          throw invalid_argument("Absolute price is not compatible with smart buy / sell");
        }
        PriceOptions priceOptions(tradePrice);
        coincenterCommand.setTradeOptions(
            TradeOptions(priceOptions, timeoutAction, tradeMode, cmdLineOptions.tradeTimeout));
      }
    } else {
      coincenterCommand.setTradeOptions(TradeOptions(timeoutAction, tradeMode, cmdLineOptions.tradeTimeout,
                                                     cmdLineOptions.tradeUpdatePrice, tradeType));
    }

    StringOptionParser optParser(tradeArgs);
    if (isSmartTrade) {
      if (!cmdLineOptions.sellAll.empty()) {
        auto [fromTradeCurrency, exchanges] = optParser.getCurrencyPrivateExchanges();
        coincenterCommand.setAmount(MonetaryAmount(100, fromTradeCurrency))
            .setPercentageAmount(true)
            .setExchangeNames(std::move(exchanges));
      } else {
        auto [amount, isPercentage, exchanges] = optParser.getMonetaryAmountPrivateExchanges();
        if (amount.isNegativeOrZero()) {
          throw invalid_argument("Start trade amount should be positive");
        }
        coincenterCommand.setAmount(amount).setPercentageAmount(isPercentage).setExchangeNames(std::move(exchanges));
      }
    } else if (isTradeAll) {
      auto [fromTradeCurrency, toTradeCurrency, exchanges] = optParser.getCurrenciesPrivateExchanges();
      coincenterCommand.setCur1(fromTradeCurrency).setCur2(toTradeCurrency).setExchangeNames(std::move(exchanges));
    } else {
      auto [startTradeAmount, isPercentage, toTradeCurrency, exchanges] =
          optParser.getMonetaryAmountCurrencyPrivateExchanges();
      if (startTradeAmount.isNegativeOrZero()) {
        throw invalid_argument("Start trade amount should be positive");
      }
      coincenterCommand.setAmount(startTradeAmount)
          .setPercentageAmount(isPercentage)
          .setCur1(toTradeCurrency)
          .setExchangeNames(std::move(exchanges));
    }
  }

  if (!cmdLineOptions.depositInfo.empty()) {
    StringOptionParser anyParser(cmdLineOptions.depositInfo);
    auto [depositCurrency, exchanges] = anyParser.getCurrencyPrivateExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kDepositInfo)
        .setCur1(depositCurrency)
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.openedOrdersInfo) {
    auto [ordersConstraints, exchanges] = ParseOrderRequest(cmdLineOptions, *cmdLineOptions.openedOrdersInfo);
    _commands.emplace_back(CoincenterCommand::Type::kOrdersOpened)
        .setOrdersConstraints(std::move(ordersConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.cancelOpenedOrders) {
    auto [ordersConstraints, exchanges] = ParseOrderRequest(cmdLineOptions, *cmdLineOptions.cancelOpenedOrders);
    _commands.emplace_back(CoincenterCommand::Type::kOrdersCancel)
        .setOrdersConstraints(std::move(ordersConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.withdraw.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdraw);
    auto [amountToWithdraw, isPercentageWithdraw, fromExchange, toExchange] =
        anyParser.getMonetaryAmountFromToPrivateExchange();
    ExchangeNames exchanges;
    exchanges.push_back(std::move(fromExchange));
    exchanges.push_back(std::move(toExchange));
    _commands.emplace_back(CoincenterCommand::Type::kWithdraw)
        .setAmount(amountToWithdraw)
        .setPercentageAmount(isPercentageWithdraw)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.withdrawAll.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawAll);
    auto [curToWithdraw, fromExchange, toExchange] = anyParser.getCurrencyFromToPrivateExchange();
    ExchangeNames exchanges;
    exchanges.push_back(std::move(fromExchange));
    exchanges.push_back(std::move(toExchange));
    _commands.emplace_back(CoincenterCommand::Type::kWithdraw)
        .setAmount(MonetaryAmount(100, curToWithdraw))
        .setPercentageAmount(true)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.withdrawFee.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawFee);
    auto [withdrawFeeCur, exchanges] = anyParser.getCurrencyPublicExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kWithdrawFee)
        .setCur1(withdrawFeeCur)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    StringOptionParser anyParser(cmdLineOptions.last24hTradedVolume);
    auto [tradedVolumeMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kLast24hTradedVolume)
        .setMarket(tradedVolumeMarket)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.lastTrades.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastTrades);
    auto [lastTradesMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kLastTrades)
        .setMarket(lastTradesMarket)
        .setNbLastTrades(cmdLineOptions.nbLastTrades)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.lastPrice.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastPrice);
    auto [lastPriceMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommand::Type::kLastPrice)
        .setMarket(lastPriceMarket)
        .setExchangeNames(std::move(exchanges));
  }

  return true;
}
}  // namespace cct
