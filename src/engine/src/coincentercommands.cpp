#include "coincentercommands.hpp"

#include <span>
#include <string_view>
#include <utility>

#include "cct_invalid_argument_exception.hpp"
#include "coincentercommand.hpp"
#include "coincentercommandfactory.hpp"
#include "coincentercommandtype.hpp"
#include "coincenteroptions.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "market.hpp"
#include "replay-options.hpp"
#include "stringoptionparser.hpp"
#include "time-window.hpp"
#include "timedef.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {

CoincenterCommands::CoincenterCommands(std::span<const CoincenterCmdLineOptions> cmdLineOptionsSpan) {
  _commands.reserve(static_cast<Commands::size_type>(cmdLineOptionsSpan.size()));
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
    _commands.emplace_back(CoincenterCommandType::HealthCheck).setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.currencies) {
    optionParser = StringOptionParser(*cmdLineOptions.currencies);
    _commands.emplace_back(CoincenterCommandType::Currencies).setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.markets) {
    optionParser = StringOptionParser(*cmdLineOptions.markets);
    _commands.push_back(CoincenterCommandFactory::CreateMarketCommand(optionParser));
  }

  if (!cmdLineOptions.orderbook.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.orderbook);
    auto &cmd = _commands.emplace_back(CoincenterCommandType::Orderbook)
                    .setMarket(optionParser.parseMarket())
                    .setExchangeNames(optionParser.parseExchanges())
                    .setCur1(cmdLineOptions.orderbookCur);
    if (cmdLineOptions.depth != CoincenterCmdLineOptions::kUndefinedDepth) {
      cmd.setDepth(cmdLineOptions.depth);
    }
  }

  if (cmdLineOptions.ticker) {
    optionParser = StringOptionParser(*cmdLineOptions.ticker);
    _commands.emplace_back(CoincenterCommandType::Ticker).setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.conversion.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.conversion);

    const auto [amount, amountType] = optionParser.parseNonZeroAmount(StringOptionParser::FieldIs::kOptional);
    if (amountType == StringOptionParser::AmountType::kPercentage) {
      throw invalid_argument("conversion should start with an absolute amount");
    }
    _commands.emplace_back(CoincenterCommandType::Conversion)
        .setAmount(amount)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.conversionPath.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.conversionPath);
    _commands.emplace_back(CoincenterCommandType::ConversionPath)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.balance) {
    optionParser = StringOptionParser(*cmdLineOptions.balance);
    _commands.emplace_back(CoincenterCommandType::Balance)
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
    _commands.emplace_back(CoincenterCommandType::DepositInfo)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.closedOrdersInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.closedOrdersInfo);
    _commands.push_back(commandFactory.createOrderCommand(CoincenterCommandType::OrdersClosed, optionParser));
  }

  if (cmdLineOptions.openedOrdersInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.openedOrdersInfo);
    _commands.push_back(commandFactory.createOrderCommand(CoincenterCommandType::OrdersOpened, optionParser));
  }

  if (cmdLineOptions.cancelOpenedOrders) {
    optionParser = StringOptionParser(*cmdLineOptions.cancelOpenedOrders);
    _commands.push_back(commandFactory.createOrderCommand(CoincenterCommandType::OrdersCancel, optionParser));
  }

  if (cmdLineOptions.recentDepositsInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.recentDepositsInfo);
    _commands.emplace_back(CoincenterCommandType::RecentDeposits)
        .setDepositsConstraints(DepositsConstraints(
            optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional), cmdLineOptions.minAge,
            cmdLineOptions.maxAge, DepositsConstraints::IdSet(StringOptionParser(cmdLineOptions.ids).getCSVValues())))
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.recentWithdrawsInfo) {
    optionParser = StringOptionParser(*cmdLineOptions.recentWithdrawsInfo);
    _commands.emplace_back(CoincenterCommandType::RecentWithdraws)
        .setWithdrawsConstraints(WithdrawsConstraints(
            optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional), cmdLineOptions.minAge,
            cmdLineOptions.maxAge, WithdrawsConstraints::IdSet(StringOptionParser(cmdLineOptions.ids).getCSVValues())))
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
    _commands.emplace_back(CoincenterCommandType::DustSweeper)
        .setCur1(optionParser.parseCurrency())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.withdrawFees) {
    optionParser = StringOptionParser(*cmdLineOptions.withdrawFees);
    _commands.emplace_back(CoincenterCommandType::WithdrawFees)
        .setCur1(optionParser.parseCurrency(StringOptionParser::FieldIs::kOptional))
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.last24hTradedVolume.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.last24hTradedVolume);
    _commands.emplace_back(CoincenterCommandType::Last24hTradedVolume)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.lastTrades.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.lastTrades);
    auto &cmd = _commands.emplace_back(CoincenterCommandType::LastTrades)
                    .setMarket(optionParser.parseMarket())
                    .setExchangeNames(optionParser.parseExchanges());
    if (cmdLineOptions.depth != CoincenterCmdLineOptions::kUndefinedDepth) {
      cmd.setDepth(cmdLineOptions.depth);
    }
  }

  if (!cmdLineOptions.lastPrice.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.lastPrice);
    _commands.emplace_back(CoincenterCommandType::LastPrice)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (!cmdLineOptions.marketData.empty()) {
    optionParser = StringOptionParser(cmdLineOptions.marketData);

    _commands.emplace_back(CoincenterCommandType::MarketData)
        .setMarket(optionParser.parseMarket())
        .setExchangeNames(optionParser.parseExchanges());
  }

  if (cmdLineOptions.replay) {
    optionParser = StringOptionParser(*cmdLineOptions.replay);

    auto dur = optionParser.parseDuration(StringOptionParser::FieldIs::kOptional);

    auto &cmd = _commands.emplace_back(CoincenterCommandType::Replay)
                    .setReplayOptions(cmdLineOptions.computeReplayOptions(dur))
                    .setExchangeNames(optionParser.parseExchanges());

    if (!cmdLineOptions.market.empty()) {
      cmd.setMarket(Market(cmdLineOptions.market));
    }
  }

  if (cmdLineOptions.replayMarkets) {
    optionParser = StringOptionParser(*cmdLineOptions.replayMarkets);

    TimeWindow timeWindow;
    auto dur = optionParser.parseDuration(StringOptionParser::FieldIs::kOptional);
    auto nowTime = Clock::now();
    if (dur == kUndefinedDuration) {
      timeWindow = TimeWindow(TimePoint{}, nowTime);
    } else {
      timeWindow = TimeWindow(nowTime - dur, nowTime);
    }

    _commands.emplace_back(CoincenterCommandType::ReplayMarkets)
        .setReplayOptions(
            ReplayOptions(timeWindow, cmdLineOptions.algorithmNames, ReplayOptions::ReplayMode::kValidateOnly))
        .setExchangeNames(optionParser.parseExchanges());
  }

  optionParser.checkEndParsing();  // No more option part should be remaining
}

}  // namespace cct
