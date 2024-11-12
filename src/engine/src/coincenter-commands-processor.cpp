#include "coincenter-commands-processor.hpp"

#include <algorithm>
#include <array>
#include <span>
#include <thread>
#include <utility>

#include "balanceoptions.hpp"
#include "cct_const.hpp"
#include "cct_exception.hpp"
#include "cct_invalid_argument_exception.hpp"
#include "cct_log.hpp"
#include "coincenter-commands-iterator.hpp"
#include "coincenter.hpp"
#include "coincentercommand.hpp"
#include "coincentercommands.hpp"
#include "coincentercommandtype.hpp"
#include "coincenterinfo.hpp"
#include "currencycode.hpp"
#include "durationstring.hpp"
#include "exchange-names.hpp"
#include "exchangename.hpp"
#include "exchangepublicapi.hpp"
#include "market-trader-factory.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "queryresultprinter.hpp"
#include "queryresulttypes.hpp"
#include "replay-options.hpp"
#include "signal-handler.hpp"
#include "timedef.hpp"
#include "transferablecommandresult.hpp"

namespace cct {
namespace {

void FillTradeTransferableCommandResults(const TradeResultPerExchange &tradeResultPerExchange,
                                         TransferableCommandResultVector &transferableResults) {
  for (const auto &[exchangePtr, tradeResult] : tradeResultPerExchange) {
    if (tradeResult.isComplete()) {
      transferableResults.emplace_back(exchangePtr->createExchangeName(), tradeResult.tradedAmounts().to);
    }
  }
}

void FillConversionTransferableCommandResults(const MonetaryAmountPerExchange &monetaryAmountPerExchange,
                                              TransferableCommandResultVector &transferableResults) {
  for (const auto &[exchangePtr, amount] : monetaryAmountPerExchange) {
    transferableResults.emplace_back(exchangePtr->createExchangeName(), amount);
  }
}

}  // namespace

CoincenterCommandsProcessor::CoincenterCommandsProcessor(Coincenter &coincenter)
    : _coincenter(coincenter),
      _queryResultPrinter(coincenter.coincenterInfo().apiOutputType(), coincenter.coincenterInfo().loggingInfo()) {}

int CoincenterCommandsProcessor::process(const CoincenterCommands &coincenterCommands) {
  const auto commands = coincenterCommands.commands();
  const int nbRepeats = commands.empty() ? 0 : coincenterCommands.repeats();
  const auto repeatTime = coincenterCommands.repeatTime();

  int nbCommandsProcessed{};
  TimePoint lastCommandTime;
  for (int repeatPos{}; repeatPos != nbRepeats && !IsStopRequested(); ++repeatPos) {
    const auto earliestTimeNextCommand = lastCommandTime + repeatTime;
    const bool doLog = nbRepeats != 1 && (repeatPos < 100 || repeatPos % 100 == 0);

    lastCommandTime = Clock::now();

    if (lastCommandTime < earliestTimeNextCommand) {
      const auto waitingDuration = earliestTimeNextCommand - lastCommandTime;

      lastCommandTime += waitingDuration;

      if (doLog) {
        log::debug("Sleep for {} before next command", DurationToString(waitingDuration));
      }
      std::this_thread::sleep_for(waitingDuration);
    }
    if (doLog) {
      if (nbRepeats == -1) {
        log::info("Process request {}", repeatPos + 1);
      } else {
        log::info("Process request {}/{}", repeatPos + 1, nbRepeats);
      }
    }
    TransferableCommandResultVector transferableResults;
    CoincenterCommandsIterator commandsIterator(commands);
    while (commandsIterator.hasNextCommandGroup()) {
      const auto groupedCommands = commandsIterator.nextCommandGroup();
      transferableResults = processGroupedCommands(groupedCommands, transferableResults);
      ++nbCommandsProcessed;
    }
  }
  return nbCommandsProcessed;
}

TransferableCommandResultVector CoincenterCommandsProcessor::processGroupedCommands(
    std::span<const CoincenterCommand> groupedCommands,
    std::span<const TransferableCommandResult> previousTransferableResults) {
  TransferableCommandResultVector transferableResults;
  const auto &firstCmd = groupedCommands.front();
  // All grouped commands have same type - logic to handle multiple commands in a group should be handled per use case
  switch (firstCmd.type()) {
    case CoincenterCommandType::HealthCheck: {
      const auto healthCheckStatus = _coincenter.healthCheck(firstCmd.exchangeNames());
      _queryResultPrinter.printHealthCheck(healthCheckStatus);
      break;
    }
    case CoincenterCommandType::Currencies: {
      const auto currenciesPerExchange = _coincenter.getCurrenciesPerExchange(firstCmd.exchangeNames());
      _queryResultPrinter.printCurrencies(currenciesPerExchange);
      break;
    }
    case CoincenterCommandType::Markets: {
      const auto marketsPerExchange =
          _coincenter.getMarketsPerExchange(firstCmd.cur1(), firstCmd.cur2(), firstCmd.exchangeNames());
      _queryResultPrinter.printMarkets(firstCmd.cur1(), firstCmd.cur2(), marketsPerExchange, firstCmd.type());
      break;
    }
    case CoincenterCommandType::Conversion: {
      ExchangeNameEnumVector exchangeNameEnumVector;
      if (firstCmd.amount().isDefault()) {
        std::array<MonetaryAmount, kNbSupportedExchanges> startAmountsPerExchangePos;
        bool oneSet = false;
        for (const auto &transferableResult : previousTransferableResults) {
          auto publicExchangePos = transferableResult.targetedExchange().publicExchangePos();
          if (startAmountsPerExchangePos[publicExchangePos].isDefault()) {
            startAmountsPerExchangePos[publicExchangePos] = transferableResult.resultedAmount();
            exchangeNameEnumVector.push_back(static_cast<ExchangeNameEnum>(publicExchangePos));
            oneSet = true;
          } else {
            throw invalid_argument(
                "Transferable results to conversion should have at most one amount per public exchange");
          }
        }
        if (!oneSet) {
          throw invalid_argument("Missing input amount to convert from");
        }

        const auto conversionPerExchange =
            _coincenter.getConversion(startAmountsPerExchangePos, firstCmd.cur1(), exchangeNameEnumVector);
        _queryResultPrinter.printConversion(startAmountsPerExchangePos, firstCmd.cur1(), conversionPerExchange);
        FillConversionTransferableCommandResults(conversionPerExchange, transferableResults);
      } else {
        const auto conversionPerExchange =
            _coincenter.getConversion(firstCmd.amount(), firstCmd.cur1(), exchangeNameEnumVector);
        _queryResultPrinter.printConversion(firstCmd.amount(), firstCmd.cur1(), conversionPerExchange);
        FillConversionTransferableCommandResults(conversionPerExchange, transferableResults);
      }
      break;
    }
    case CoincenterCommandType::ConversionPath: {
      const auto conversionPathPerExchange =
          _coincenter.getConversionPaths(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printConversionPath(firstCmd.market(), conversionPathPerExchange);
      break;
    }
    case CoincenterCommandType::LastPrice: {
      const auto lastPricePerExchange =
          _coincenter.getLastPricePerExchange(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printLastPrice(firstCmd.market(), lastPricePerExchange);
      break;
    }
    case CoincenterCommandType::Ticker: {
      const auto exchangeTickerMaps = _coincenter.getTickerInformation(firstCmd.exchangeNames());
      _queryResultPrinter.printTickerInformation(exchangeTickerMaps);
      break;
    }
    case CoincenterCommandType::Orderbook: {
      const auto marketOrderBooksConversionRates = _coincenter.getMarketOrderBooks(
          firstCmd.market(), firstCmd.exchangeNames(), firstCmd.cur1(), firstCmd.optDepth());
      _queryResultPrinter.printMarketOrderBooks(firstCmd.market(), firstCmd.cur1(), firstCmd.optDepth(),
                                                marketOrderBooksConversionRates);
      break;
    }
    case CoincenterCommandType::LastTrades: {
      const auto lastTradesPerExchange =
          _coincenter.getLastTradesPerExchange(firstCmd.market(), firstCmd.exchangeNames(), firstCmd.optDepth());
      _queryResultPrinter.printLastTrades(firstCmd.market(), firstCmd.optDepth(), lastTradesPerExchange);
      break;
    }
    case CoincenterCommandType::Last24hTradedVolume: {
      const auto tradedVolumePerExchange =
          _coincenter.getLast24hTradedVolumePerExchange(firstCmd.market(), firstCmd.exchangeNames());
      _queryResultPrinter.printLast24hTradedVolume(firstCmd.market(), tradedVolumePerExchange);
      break;
    }
    case CoincenterCommandType::WithdrawFees: {
      const auto withdrawFeesPerExchange = _coincenter.getWithdrawFees(firstCmd.cur1(), firstCmd.exchangeNames());
      _queryResultPrinter.printWithdrawFees(withdrawFeesPerExchange, firstCmd.cur1());
      break;
    }

    case CoincenterCommandType::Balance: {
      const auto amountIncludePolicy = firstCmd.withBalanceInUse()
                                           ? BalanceOptions::AmountIncludePolicy::kWithBalanceInUse
                                           : BalanceOptions::AmountIncludePolicy::kOnlyAvailable;
      const BalanceOptions balanceOptions(amountIncludePolicy, firstCmd.cur1());
      const auto balancePerExchange = _coincenter.getBalance(firstCmd.exchangeNames(), balanceOptions);
      _queryResultPrinter.printBalance(balancePerExchange, firstCmd.cur1());
      break;
    }
    case CoincenterCommandType::DepositInfo: {
      const auto walletPerExchange = _coincenter.getDepositInfo(firstCmd.exchangeNames(), firstCmd.cur1());
      _queryResultPrinter.printDepositInfo(firstCmd.cur1(), walletPerExchange);
      break;
    }
    case CoincenterCommandType::OrdersClosed: {
      const auto closedOrdersPerExchange =
          _coincenter.getClosedOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printClosedOrders(closedOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::OrdersOpened: {
      const auto openedOrdersPerExchange =
          _coincenter.getOpenedOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printOpenedOrders(openedOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::OrdersCancel: {
      const auto nbCancelledOrdersPerExchange =
          _coincenter.cancelOrders(firstCmd.exchangeNames(), firstCmd.ordersConstraints());
      _queryResultPrinter.printCancelledOrders(nbCancelledOrdersPerExchange, firstCmd.ordersConstraints());
      break;
    }
    case CoincenterCommandType::RecentDeposits: {
      const auto depositsPerExchange =
          _coincenter.getRecentDeposits(firstCmd.exchangeNames(), firstCmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentDeposits(depositsPerExchange, firstCmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::RecentWithdraws: {
      const auto withdrawsPerExchange =
          _coincenter.getRecentWithdraws(firstCmd.exchangeNames(), firstCmd.withdrawsOrDepositsConstraints());
      _queryResultPrinter.printRecentWithdraws(withdrawsPerExchange, firstCmd.withdrawsOrDepositsConstraints());
      break;
    }
    case CoincenterCommandType::Trade: {
      // 2 input styles are possible:
      //  - standard full information with an amount to trade, a destination currency and an optional list of exchanges
      //  where to trade
      //  - a currency - the destination one, and start amount and exchange(s) should come from previous command result
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(firstCmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange = _coincenter.trade(startAmount, firstCmd.isPercentageAmount(), firstCmd.cur1(),
                                                            exchangeNames, firstCmd.tradeOptions());
      _queryResultPrinter.printTrades(tradeResultPerExchange, startAmount, firstCmd.isPercentageAmount(),
                                      firstCmd.cur1(), firstCmd.tradeOptions());
      FillTradeTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::Buy: {
      const auto tradeResultPerExchange =
          _coincenter.smartBuy(firstCmd.amount(), firstCmd.exchangeNames(), firstCmd.tradeOptions());
      _queryResultPrinter.printBuyTrades(tradeResultPerExchange, firstCmd.amount(), firstCmd.tradeOptions());
      FillTradeTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::Sell: {
      auto [startAmount, exchangeNames] = ComputeTradeAmountAndExchanges(firstCmd, previousTransferableResults);
      if (startAmount.isDefault()) {
        break;
      }
      const auto tradeResultPerExchange =
          _coincenter.smartSell(startAmount, firstCmd.isPercentageAmount(), exchangeNames, firstCmd.tradeOptions());
      _queryResultPrinter.printSellTrades(tradeResultPerExchange, firstCmd.amount(), firstCmd.isPercentageAmount(),
                                          firstCmd.tradeOptions());
      FillTradeTransferableCommandResults(tradeResultPerExchange, transferableResults);
      break;
    }
    case CoincenterCommandType::Withdraw: {
      const auto [grossAmount, exchangeName] = ComputeWithdrawAmount(firstCmd, previousTransferableResults);
      if (grossAmount.isDefault()) {
        break;
      }
      const auto deliveredWithdrawInfoWithExchanges =
          _coincenter.withdraw(grossAmount, firstCmd.isPercentageAmount(), exchangeName,
                               firstCmd.exchangeNames().back(), firstCmd.withdrawOptions());
      _queryResultPrinter.printWithdraw(deliveredWithdrawInfoWithExchanges, firstCmd.isPercentageAmount(),
                                        firstCmd.withdrawOptions());
      transferableResults.emplace_back(deliveredWithdrawInfoWithExchanges.first[1]->createExchangeName(),
                                       deliveredWithdrawInfoWithExchanges.second.receivedAmount());
      break;
    }
    case CoincenterCommandType::DustSweeper: {
      const auto res = _coincenter.dustSweeper(firstCmd.exchangeNames(), firstCmd.cur1());
      _queryResultPrinter.printDustSweeper(res, firstCmd.cur1());
      break;
    }
    case CoincenterCommandType::MarketData: {
      std::array<Market, kNbSupportedExchanges> marketPerPublicExchange;
      for (const auto &cmd : groupedCommands) {
        if (cmd.exchangeNames().empty()) {
          std::ranges::fill(marketPerPublicExchange, cmd.market());
        } else {
          for (const auto &exchangeName : cmd.exchangeNames()) {
            marketPerPublicExchange[exchangeName.publicExchangePos()] = cmd.market();
          }
        }
      }
      // No return value here, this command is made only for storing purposes.
      _coincenter.queryMarketDataPerExchange(marketPerPublicExchange);
      break;
    }
    case CoincenterCommandType::Replay: {
      /// This implementation of AbstractMarketTraderFactory is only provided as an example.
      /// You can extend coincenter library and:
      ///  - Provide your own algorithms by implementing your own MarketTraderFactory will all your algorithms.
      ///  - Create your own CommandType that will call coincenter.replay with the same parameters as below, with your
      ///    own MarketTraderFactory.
      MarketTraderFactory marketTraderFactory;
      const auto replayResults = _coincenter.replay(marketTraderFactory, firstCmd.replayOptions(), firstCmd.market(),
                                                    firstCmd.exchangeNames());

      _queryResultPrinter.printMarketTradingResults(firstCmd.replayOptions().timeWindow(), replayResults,
                                                    CoincenterCommandType::Replay);

      break;
    }
    case CoincenterCommandType::ReplayMarkets: {
      const auto marketTimestampSetsPerExchange =
          _coincenter.getMarketsAvailableForReplay(firstCmd.replayOptions(), firstCmd.exchangeNames());
      _queryResultPrinter.printMarketsForReplay(firstCmd.replayOptions().timeWindow(), marketTimestampSetsPerExchange);
      break;
    }
    default:
      throw exception("Unknown command type");
  }
  return transferableResults;
}

}  // namespace cct
