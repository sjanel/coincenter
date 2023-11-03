#include "coincentercommands.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "cct_vector.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "coincenteroptionsdef.hpp"
#include "commandlineoptionsparser.hpp"
#include "commandlineoptionsparseriterator.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "stringoptionparser.hpp"
#include "timedef.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

vector<CoincenterCmdLineOptions> CoincenterCommands::ParseOptions(int argc, const char *argv[]) {
  using OptValueType = CoincenterCmdLineOptions;

  auto parser = CommandLineOptionsParser<OptValueType>(CoincenterAllowedOptions<OptValueType>::value);

  auto programName = std::filesystem::path(argv[0]).filename().string();

  vector<CoincenterCmdLineOptions> parsedOptions;

  std::span<const char *> allArguments(argv, argc);
  allArguments = allArguments.last(allArguments.size() - 1U);  // skip first argument which is program name

  CommandLineOptionsParserIterator parserIt(parser, allArguments);

  CoincenterCmdLineOptions globalOptions;

  // Support for command line multiple commands. Only full name flags are supported for multi command line commands.
  while (parserIt.hasNext()) {
    std::span<const char *> groupedArguments = parserIt.next();

    CoincenterCmdLineOptions groupParsedOptions = parser.parse(groupedArguments);
    globalOptions.mergeGlobalWith(groupParsedOptions);

    if (groupedArguments.empty()) {
      groupParsedOptions.help = true;
    }
    if (groupParsedOptions.help) {
      parser.displayHelp(programName, std::cout);
    } else if (groupParsedOptions.version) {
      CoincenterCmdLineOptions::PrintVersion(programName, std::cout);
    } else {
      // Only store commands if they are not 'help' nor 'version'
      parsedOptions.push_back(std::move(groupParsedOptions));
    }
  }

  // Apply global options to all parsed options containing commands
  for (CoincenterCmdLineOptions &groupParsedOptions : parsedOptions) {
    groupParsedOptions.mergeGlobalWith(globalOptions);
  }

  return parsedOptions;
}

namespace {
std::pair<OrdersConstraints, ExchangeNames> ParseOrderRequest(const CoincenterCmdLineOptions &cmdLineOptions,
                                                              std::string_view orderRequestStr) {
  auto currenciesPrivateExchangesTuple = StringOptionParser(orderRequestStr).getCurrenciesPrivateExchanges(false);
  auto orderIds = StringOptionParser(cmdLineOptions.ids).getCSVValues();
  return std::make_pair(
      OrdersConstraints(std::get<0>(currenciesPrivateExchangesTuple), std::get<1>(currenciesPrivateExchangesTuple),
                        std::chrono::duration_cast<Duration>(cmdLineOptions.minAge),
                        std::chrono::duration_cast<Duration>(cmdLineOptions.maxAge),
                        OrdersConstraints::OrderIdSet(std::move(orderIds))),
      std::get<2>(currenciesPrivateExchangesTuple));
}

}  // namespace

CoincenterCommands::CoincenterCommands(std::span<const CoincenterCmdLineOptions> cmdLineOptionsSpan) {
  for (const CoincenterCmdLineOptions &cmdLineOptions : cmdLineOptionsSpan) {
    addOption(cmdLineOptions);
  }
}

bool CoincenterCommands::addOption(const CoincenterCmdLineOptions &cmdLineOptions) {
  if (cmdLineOptions.repeats.isPresent()) {
    if (cmdLineOptions.repeats.isSet()) {
      _repeats = *cmdLineOptions.repeats;
    } else {
      // infinite repeats
      _repeats = -1;
    }
  }

  _repeatTime = cmdLineOptions.repeatTime;

  if (cmdLineOptions.healthCheck) {
    StringOptionParser anyParser(*cmdLineOptions.healthCheck);
    _commands.emplace_back(CoincenterCommandType::kHealthCheck).setExchangeNames(anyParser.getExchanges());
  }

  if (!cmdLineOptions.markets.empty()) {
    StringOptionParser anyParser(cmdLineOptions.markets);
    auto [cur1, cur2, exchanges] = anyParser.getCurrenciesPublicExchanges();
    _commands.emplace_back(CoincenterCommandType::kMarkets)
        .setCur1(cur1)
        .setCur2(cur2)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.orderbook.empty()) {
    StringOptionParser anyParser(cmdLineOptions.orderbook);
    auto [market, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommandType::kOrderbook)
        .setMarket(market)
        .setExchangeNames(std::move(exchanges))
        .setDepth(cmdLineOptions.orderbookDepth)
        .setCur1(cmdLineOptions.orderbookCur);
  }

  if (cmdLineOptions.ticker) {
    StringOptionParser anyParser(*cmdLineOptions.ticker);
    _commands.emplace_back(CoincenterCommandType::kTicker).setExchangeNames(anyParser.getExchanges());
  }

  if (!cmdLineOptions.conversionPath.empty()) {
    StringOptionParser anyParser(cmdLineOptions.conversionPath);
    auto [market, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommandType::kConversionPath)
        .setMarket(market)
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.balance) {
    StringOptionParser anyParser(*cmdLineOptions.balance);
    auto [balanceCurrencyCode, exchanges] =
        anyParser.getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kOptional);
    _commands.emplace_back(CoincenterCommandType::kBalance)
        .setCur1(balanceCurrencyCode)
        .withBalanceInUse(cmdLineOptions.withBalanceInUse)
        .setExchangeNames(std::move(exchanges));
  }

  // Parse trade / buy / sell options
  // First, check that at most one master trade option is set
  // (options would be set for all trades otherwise which is not very intuitive)
  if (static_cast<int>(!cmdLineOptions.buy.empty()) + static_cast<int>(!cmdLineOptions.sell.empty()) +
          static_cast<int>(!cmdLineOptions.sellAll.empty()) + static_cast<int>(!cmdLineOptions.tradeAll.empty()) >
      1) {
    throw invalid_argument("Only one trade can be done at a time");
  }
  auto [tradeArgs, cmdType] = cmdLineOptions.getTradeArgStr();
  if (!tradeArgs.empty()) {
    CoincenterCommand &coincenterCommand = _commands.emplace_back(cmdType);

    coincenterCommand.setTradeOptions(cmdLineOptions.computeTradeOptions());

    StringOptionParser optParser(tradeArgs);
    if (cmdLineOptions.isSmartTrade()) {
      if (!cmdLineOptions.sellAll.empty()) {
        auto [fromTradeCurrency, exchanges] =
            optParser.getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kMandatory);
        coincenterCommand.setAmount(MonetaryAmount(100, fromTradeCurrency))
            .setPercentageAmount(true)
            .setExchangeNames(std::move(exchanges));
      } else {
        auto [amount, isPercentage, exchanges] = optParser.getMonetaryAmountPrivateExchanges();
        if (amount <= 0) {
          throw invalid_argument("Start trade amount should be positive");
        }
        coincenterCommand.setAmount(amount).setPercentageAmount(isPercentage).setExchangeNames(std::move(exchanges));
      }
    } else if (!cmdLineOptions.tradeAll.empty()) {
      auto [fromTradeCurrency, toTradeCurrency, exchanges] = optParser.getCurrenciesPrivateExchanges();
      coincenterCommand.setAmount(MonetaryAmount(100, fromTradeCurrency))
          .setPercentageAmount(true)
          .setCur1(toTradeCurrency)
          .setExchangeNames(std::move(exchanges));
    } else {
      auto [startTradeAmount, isPercentage, toTradeCurrency, exchanges] =
          optParser.getMonetaryAmountCurrencyPrivateExchanges();
      if (startTradeAmount <= 0) {
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
    auto [depositCurrency, exchanges] =
        anyParser.getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kMandatory);
    _commands.emplace_back(CoincenterCommandType::kDepositInfo)
        .setCur1(depositCurrency)
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.openedOrdersInfo) {
    auto [ordersConstraints, exchanges] = ParseOrderRequest(cmdLineOptions, *cmdLineOptions.openedOrdersInfo);
    _commands.emplace_back(CoincenterCommandType::kOrdersOpened)
        .setOrdersConstraints(std::move(ordersConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.cancelOpenedOrders) {
    auto [ordersConstraints, exchanges] = ParseOrderRequest(cmdLineOptions, *cmdLineOptions.cancelOpenedOrders);
    _commands.emplace_back(CoincenterCommandType::kOrdersCancel)
        .setOrdersConstraints(std::move(ordersConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.recentDepositsInfo) {
    auto [currencyCode, exchanges] = StringOptionParser(*cmdLineOptions.recentDepositsInfo)
                                         .getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kOptional);
    auto depositIds = StringOptionParser(cmdLineOptions.ids).getCSVValues();
    DepositsConstraints depositConstraints(currencyCode, std::chrono::duration_cast<Duration>(cmdLineOptions.minAge),
                                           std::chrono::duration_cast<Duration>(cmdLineOptions.maxAge),
                                           DepositsConstraints::IdSet(std::move(depositIds)));
    _commands.emplace_back(CoincenterCommandType::kRecentDeposits)
        .setDepositsConstraints(std::move(depositConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (cmdLineOptions.recentWithdrawsInfo) {
    auto [currencyCode, exchanges] = StringOptionParser(*cmdLineOptions.recentWithdrawsInfo)
                                         .getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kOptional);
    auto withdrawIds = StringOptionParser(cmdLineOptions.ids).getCSVValues();
    WithdrawsConstraints withdrawConstraints(currencyCode, std::chrono::duration_cast<Duration>(cmdLineOptions.minAge),
                                             std::chrono::duration_cast<Duration>(cmdLineOptions.maxAge),
                                             WithdrawsConstraints::IdSet(std::move(withdrawIds)));
    _commands.emplace_back(CoincenterCommandType::kRecentWithdraws)
        .setWithdrawsConstraints(std::move(withdrawConstraints))
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.withdrawApply.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawApply);
    auto [amountToWithdraw, isPercentageWithdraw, exchanges] = anyParser.getMonetaryAmountFromToPrivateExchange();
    _commands.emplace_back(CoincenterCommandType::kWithdrawApply)
        .setAmount(amountToWithdraw)
        .setPercentageAmount(isPercentageWithdraw)
        .setExchangeNames(std::move(exchanges))
        .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions());
  }

  if (!cmdLineOptions.withdrawApplyAll.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawApplyAll);
    auto [curToWithdraw, exchanges] = anyParser.getCurrencyFromToPrivateExchange();
    _commands.emplace_back(CoincenterCommandType::kWithdrawApply)
        .setAmount(MonetaryAmount(100, curToWithdraw))
        .setPercentageAmount(true)
        .setExchangeNames(std::move(exchanges))
        .setWithdrawOptions(cmdLineOptions.computeWithdrawOptions());
  }

  if (!cmdLineOptions.dustSweeper.empty()) {
    StringOptionParser anyParser(cmdLineOptions.dustSweeper);
    auto [currencyCode, exchanges] = anyParser.getCurrencyPrivateExchanges(StringOptionParser::CurrencyIs::kMandatory);
    _commands.emplace_back(CoincenterCommandType::kDustSweeper)
        .setCur1(currencyCode)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.withdrawFee.empty()) {
    StringOptionParser anyParser(cmdLineOptions.withdrawFee);
    auto [withdrawFeeCur, exchanges] = anyParser.getCurrencyPublicExchanges();
    _commands.emplace_back(CoincenterCommandType::kWithdrawFee)
        .setCur1(withdrawFeeCur)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    StringOptionParser anyParser(cmdLineOptions.last24hTradedVolume);
    auto [tradedVolumeMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommandType::kLast24hTradedVolume)
        .setMarket(tradedVolumeMarket)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.lastTrades.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastTrades);
    auto [lastTradesMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommandType::kLastTrades)
        .setMarket(lastTradesMarket)
        .setNbLastTrades(cmdLineOptions.nbLastTrades)
        .setExchangeNames(std::move(exchanges));
  }

  if (!cmdLineOptions.lastPrice.empty()) {
    StringOptionParser anyParser(cmdLineOptions.lastPrice);
    auto [lastPriceMarket, exchanges] = anyParser.getMarketExchanges();
    _commands.emplace_back(CoincenterCommandType::kLastPrice)
        .setMarket(lastPriceMarket)
        .setExchangeNames(std::move(exchanges));
  }

  return true;
}

}  // namespace cct
