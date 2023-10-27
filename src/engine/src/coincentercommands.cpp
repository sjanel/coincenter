#include "coincentercommands.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>

#include "cct_vector.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandfactory.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "coincenteroptionsdef.hpp"
#include "commandlineoptionsparser.hpp"
#include "commandlineoptionsparseriterator.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
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

CoincenterCommands::CoincenterCommands(std::span<const CoincenterCmdLineOptions> cmdLineOptionsSpan) {
  _commands.reserve(cmdLineOptionsSpan.size());
  const CoincenterCommand *pPreviousCommand = nullptr;
  for (const CoincenterCmdLineOptions &cmdLineOptions : cmdLineOptionsSpan) {
    addOption(cmdLineOptions, pPreviousCommand);
    if (!_commands.empty()) {
      pPreviousCommand = &_commands.back();
    }
  }
}

void CoincenterCommands::addOption(const CoincenterCmdLineOptions &cmdLineOptions,
                                   const CoincenterCommand *pPreviousCommand) {
  // Warning: pPreviousCommand is a pointer into an object in _commands. Do not use after insertion of a new command
  // (pointer may be invalidated)
  if (cmdLineOptions.repeats.isPresent()) {
    if (cmdLineOptions.repeats.isSet()) {
      _repeats = *cmdLineOptions.repeats;
    } else {
      // infinite repeats
      _repeats = -1;
    }
  }

  _repeatTime = cmdLineOptions.repeatTime;

  StringOptionParser optionParser;
  CoincenterCommandFactory commandFactory(cmdLineOptions, pPreviousCommand);

  if (cmdLineOptions.healthCheck) {
    optionParser = StringOptionParser(*cmdLineOptions.healthCheck);
    _commands.emplace_back(CoincenterCommandType::kHealthCheck).setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.markets.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.markets);
    _commands.push_back(CoincenterCommandFactory::CreateMarketCommand(optionParser));
  }

  if (!cmdLineOptions.orderbook.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.orderbook);
    _commands.emplace_back(CoincenterCommandType::kOrderbook)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges())
        .setDepth(cmdLineOptions.orderbookDepth)
        .setCur1(cmdLineOptions.orderbookCur);
  }

  if (cmdLineOptions.ticker) {
    optionParser = StringOptionParser(*cmdLineOptions.ticker);
    _commands.emplace_back(CoincenterCommandType::kTicker).setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.conversionPath.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.conversionPath);
    _commands.emplace_back(CoincenterCommandType::kConversionPath)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.balance) {
    optionParser = StringOptionParser(*cmdLineOptions.balance);
    _commands.emplace_back(CoincenterCommandType::kBalance)
        .setCur1(optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional))
        .withBalanceInUse(cmdLineOptions.withBalanceInUse)
        .setExchangeNames(optionParser.parseExchanges());
  }

  auto [tradeArgs, cmdType] = cmdLineOptions.getTradeArgStr();
  if (!tradeArgs.empty()) {
    optionParser = StringOptionParser(tradeArgs);
    _commands.push_back(commandFactory.createTradeCommand(cmdType, optionParser));
  }

  if (!cmdLineOptions.depositInfo.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.depositInfo);
    _commands.emplace_back(CoincenterCommandType::kDepositInfo)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.openedOrdersInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.openedOrdersInfo);
    _commands.push_back(commandFactory.createOrderCommand(CoincenterCommandType::kOrdersOpened, optionParser));
  }

  if (cmdLineOptions.cancelOpenedOrders) {
    optionParser = StringOptionParser(*cmdLineOptions.cancelOpenedOrders);
    _commands.push_back(commandFactory.createOrderCommand(CoincenterCommandType::kOrdersCancel, optionParser));
  }

  if (cmdLineOptions.recentDepositsInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.recentDepositsInfo);
    _commands.emplace_back(CoincenterCommandType::kRecentDeposits)
        .setDepositsConstraints(
            DepositsConstraints(optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional),
                                std::chrono::duration_cast<Duration>(cmdLineOptions.minAge),
                                std::chrono::duration_cast<Duration>(cmdLineOptions.maxAge),
                                DepositsConstraints::IdSet(StringOptionParser(cmdLineOptions.ids).getCSVValues())))
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.recentWithdrawsInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.recentWithdrawsInfo);
    _commands.emplace_back(CoincenterCommandType::kRecentWithdraws)
        .setWithdrawsConstraints(
            WithdrawsConstraints(optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional),
                                 std::chrono::duration_cast<Duration>(cmdLineOptions.minAge),
                                 std::chrono::duration_cast<Duration>(cmdLineOptions.maxAge),
                                 WithdrawsConstraints::IdSet(StringOptionParser(cmdLineOptions.ids).getCSVValues())))
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.withdrawApply.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.withdrawApply);
    _commands.push_back(commandFactory.createWithdrawApplyCommand(optionParser));
  }

  if (!cmdLineOptions.withdrawApplyAll.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.withdrawApplyAll);
    _commands.push_back(commandFactory.createWithdrawApplyAllCommand(optionParser));
  }

  if (!cmdLineOptions.dustSweeper.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.dustSweeper);
    _commands.emplace_back(CoincenterCommandType::kDustSweeper)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.withdrawFee.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.withdrawFee);
    _commands.emplace_back(CoincenterCommandType::kWithdrawFee)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.last24hTradedVolume);
    _commands.emplace_back(CoincenterCommandType::kLast24hTradedVolume)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.lastTrades.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.lastTrades);
    _commands.emplace_back(CoincenterCommandType::kLastTrades)
        .setMarket(optionParser.parseMarket())
        .setNbLastTrades(cmdLineOptions.nbLastTrades)
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.lastPrice.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.lastPrice);
    _commands.emplace_back(CoincenterCommandType::kLastPrice)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  optionParser.checkEndParsing();  // No more option part should be remaining
}

}  // namespace cct
