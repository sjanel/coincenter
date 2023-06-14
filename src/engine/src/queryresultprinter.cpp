#include "queryresultprinter.hpp"

#include <sstream>

#include "balanceperexchangeportfolio.hpp"
#include "cct_string.hpp"
#include "coincentercommandtype.hpp"
#include "durationstring.hpp"
#include "exchange.hpp"
#include "logginginfo.hpp"
#include "simpletable.hpp"
#include "stringhelpers.hpp"
#include "timestring.hpp"
#include "tradedamounts.hpp"
#include "unreachable.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"

namespace cct {
QueryResultPrinter::QueryResultPrinter(ApiOutputType apiOutputType)
    : _outputLogger(log::get(LoggingInfo::kOutputLoggerName)), _apiOutputType(apiOutputType) {}

QueryResultPrinter::QueryResultPrinter(std::ostream &os, ApiOutputType apiOutputType)
    : _pOs(&os), _outputLogger(log::get(LoggingInfo::kOutputLoggerName)), _apiOutputType(apiOutputType) {}

void QueryResultPrinter::printHealthCheck(const ExchangeHealthCheckStatus &healthCheckPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Health Check status");
      for (const auto &[e, healthCheckValue] : healthCheckPerExchange) {
        simpleTable.emplace_back(e->name(), healthCheckValue ? "OK" : "Not OK!");
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kHealthCheck));

      json out = json::object();
      for (const auto &[e, healthCheckValue] : healthCheckPerExchange) {
        out.emplace(e->name(), healthCheckValue);
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printMarkets(CurrencyCode cur1, CurrencyCode cur2,
                                      const MarketsPerExchange &marketsPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string marketsCol("Markets with ");
      cur1.appendStrTo(marketsCol);
      if (!cur2.isNeutral()) {
        marketsCol.push_back('-');
        cur2.appendStrTo(marketsCol);
      }
      SimpleTable simpleTable("Exchange", std::move(marketsCol));
      for (const auto &[e, markets] : marketsPerExchange) {
        for (Market mk : markets) {
          simpleTable.emplace_back(e->name(), mk.str());
        }
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kMarkets));
      json inOpt;
      inOpt.emplace("cur1", cur1.str());
      if (!cur2.isNeutral()) {
        inOpt.emplace("cur2", cur2.str());
      }
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[e, markets] : marketsPerExchange) {
        json marketsForExchange;
        for (const Market &mk : markets) {
          marketsForExchange.emplace_back(mk.str());
        }
        out.emplace(e->name(), std::move(marketsForExchange));
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Market", "Bid price", "Bid volume", "Ask price", "Ask volume");
      for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
        for (const auto &[mk, marketOrderBook] : marketOrderBookMap) {
          simpleTable.emplace_back(e->name(), mk.str(), marketOrderBook.highestBidPrice().str(),
                                   marketOrderBook.amountAtBidPrice().str(), marketOrderBook.lowestAskPrice().str(),
                                   marketOrderBook.amountAtAskPrice().str());
        }
        // Sort rows in lexicographical order for consistent output
        std::sort(simpleTable.begin(), simpleTable.end());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kTicker));

      json out = json::object();
      for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
        json allTickerForExchange;
        for (const auto &[mk, marketOrderBook] : marketOrderBookMap) {
          json tickerForExchange;
          tickerForExchange.emplace("pair", mk.str());
          json ask;
          json bid;
          ask.emplace("a", marketOrderBook.amountAtAskPrice().amountStr());
          ask.emplace("p", marketOrderBook.lowestAskPrice().amountStr());
          bid.emplace("a", marketOrderBook.amountAtBidPrice().amountStr());
          bid.emplace("p", marketOrderBook.highestBidPrice().amountStr());
          tickerForExchange.emplace("ask", std::move(ask));
          tickerForExchange.emplace("bid", std::move(bid));
          allTickerForExchange.emplace_back(tickerForExchange);
        }
        // Sort rows by market pair for consistent output
        std::sort(allTickerForExchange.begin(), allTickerForExchange.end(), [](const json &lhs, const json &rhs) {
          return lhs["pair"].get<std::string_view>() < rhs["pair"].get<std::string_view>();
        });
        out.emplace(e->name(), std::move(allTickerForExchange));
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

namespace {
void AppendOrderbookLine(const MarketOrderBook &marketOrderBook, int pos,
                         std::optional<MonetaryAmount> optConversionRate, json &data) {
  auto [p, a] = marketOrderBook[pos];
  json &line = data.emplace_back();
  line.emplace("a", a.amountStr());
  line.emplace("p", p.amountStr());
  if (optConversionRate) {
    line.emplace("eq", optConversionRate->amountStr());
  }
}
}  // namespace

void QueryResultPrinter::printMarketOrderBooks(
    Market mk, CurrencyCode equiCurrencyCode, std::optional<int> depth,
    const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
        printTable(marketOrderBook.getTable(exchangeName, optConversionRate));
      }
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kOrderbook));
      json inOpt;
      inOpt.emplace("pair", mk.str());
      if (!equiCurrencyCode.isNeutral()) {
        inOpt.emplace("equiCurrency", equiCurrencyCode.str());
      }
      if (depth) {
        inOpt.emplace("depth", *depth);
      }
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
        json marketOrderBookForExchange;
        json bidsForExchange;
        json asksForExchange;
        for (int bidPos = 1; bidPos <= marketOrderBook.nbBidPrices(); ++bidPos) {
          AppendOrderbookLine(marketOrderBook, -bidPos, optConversionRate, bidsForExchange);
        }
        marketOrderBookForExchange.emplace("bid", std::move(bidsForExchange));
        for (int askPos = 1; askPos <= marketOrderBook.nbAskPrices(); ++askPos) {
          AppendOrderbookLine(marketOrderBook, askPos, optConversionRate, asksForExchange);
        }
        marketOrderBookForExchange.emplace("ask", std::move(asksForExchange));
        out.emplace(exchangeName, std::move(marketOrderBookForExchange));
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printBalance(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const {
  BalancePerExchangePortfolio totalBalance(balancePerExchange);
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      printTable(totalBalance.getTable(balancePerExchange.size() > 1));
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kBalance));
      json inOpt = json::object();
      if (!equiCurrency.isNeutral()) {
        inOpt.emplace("equiCurrency", equiCurrency.str());
      }
      in.emplace("opt", std::move(inOpt));

      printJson(std::move(in), totalBalance.printJson(equiCurrency));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printDepositInfo(CurrencyCode depositCurrencyCode,
                                          const WalletPerExchange &walletPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string walletStr(depositCurrencyCode.str());
      walletStr.append(" address");
      SimpleTable simpleTable("Exchange", "Account", std::move(walletStr), "Destination Tag");
      for (const auto &[exchangePtr, wallet] : walletPerExchange) {
        simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), wallet.address(), wallet.tag());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kDepositInfo));
      json inOpt;
      inOpt.emplace("cur", depositCurrencyCode.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[exchangePtr, wallet] : walletPerExchange) {
        json depositPerExchangeData;

        depositPerExchangeData.emplace("address", wallet.address());
        if (wallet.hasTag()) {
          depositPerExchangeData.emplace("tag", wallet.tag());
        }

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json depositInfoForExchangeUser;
          depositInfoForExchangeUser.emplace(exchangePtr->keyName(), std::move(depositPerExchangeData));
          out.emplace(exchangePtr->name(), std::move(depositInfoForExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(depositPerExchangeData));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

namespace {
inline const char *TradeModeToStr(TradeMode tradeMode) { return tradeMode == TradeMode::kReal ? "real" : "simulation"; }

json TradeOptionsToJson(const TradeOptions &tradeOptions) {
  json priceOptionsJson;
  const PriceOptions &priceOptions = tradeOptions.priceOptions();
  priceOptionsJson.emplace("strategy", priceOptions.priceStrategyStr(false));
  if (priceOptions.isFixedPrice()) {
    priceOptionsJson.emplace("fixedPrice", priceOptions.fixedPrice().str());
  }
  if (priceOptions.isRelativePrice()) {
    priceOptionsJson.emplace("relativePrice", priceOptions.relativePrice());
  }
  json ret;
  ret.emplace("price", std::move(priceOptionsJson));
  ret.emplace("maxTradeTime", DurationToString(tradeOptions.maxTradeTime()));
  ret.emplace("minTimeBetweenPriceUpdates", DurationToString(tradeOptions.minTimeBetweenPriceUpdates()));
  ret.emplace("mode", TradeModeToStr(tradeOptions.tradeMode()));
  ret.emplace("timeoutAction", tradeOptions.timeoutActionStr());
  ret.emplace("syncPolicy", tradeOptions.tradeSyncPolicyStr());
  return ret;
}
}  // namespace

void QueryResultPrinter::printTrades(const TradedAmountsPerExchange &tradedAmountsPerExchange, MonetaryAmount amount,
                                     bool isPercentageTrade, CurrencyCode toCurrency, const TradeOptions &tradeOptions,
                                     CoincenterCommandType commandType) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string tradedFromStr("Traded from amount (");
      tradedFromStr.append(TradeModeToStr(tradeOptions.tradeMode()));
      tradedFromStr.push_back(')');
      string tradedToStr("Traded to amount (");
      tradedToStr.append(TradeModeToStr(tradeOptions.tradeMode()));
      tradedToStr.push_back(')');
      SimpleTable simpleTable("Exchange", "Account", std::move(tradedFromStr), std::move(tradedToStr));
      for (const auto &[exchangePtr, tradedAmount] : tradedAmountsPerExchange) {
        simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), tradedAmount.tradedFrom.str(),
                                 tradedAmount.tradedTo.str());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(commandType));
      json fromJson;
      fromJson.emplace("amount", amount.amountStr());
      fromJson.emplace("currency", amount.currencyStr());
      fromJson.emplace("isPercentage", isPercentageTrade);

      json inOpt;
      switch (commandType) {
        case CoincenterCommandType::kBuy:
          inOpt.emplace("to", std::move(fromJson));
          break;
        case CoincenterCommandType::kSell:
          inOpt.emplace("from", std::move(fromJson));
          break;
        case CoincenterCommandType::kTrade: {
          json toJson;
          toJson.emplace("currency", toCurrency.str());

          inOpt.emplace("from", std::move(fromJson));
          inOpt.emplace("to", std::move(toJson));
          break;
        }
        default:
          unreachable();
      }

      inOpt.emplace("options", TradeOptionsToJson(tradeOptions));
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[exchangePtr, tradedAmount] : tradedAmountsPerExchange) {
        json tradedAmountPerExchangeJson;
        tradedAmountPerExchangeJson.emplace("from", tradedAmount.tradedFrom.amountStr());
        tradedAmountPerExchangeJson.emplace("to", tradedAmount.tradedTo.amountStr());

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json tradedAmountPerExchangeUser;
          tradedAmountPerExchangeUser.emplace(exchangePtr->keyName(), std::move(tradedAmountPerExchangeJson));
          out.emplace(exchangePtr->name(), std::move(tradedAmountPerExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(tradedAmountPerExchangeJson));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

namespace {
json OrdersConstraintsToJson(const OrdersConstraints &ordersConstraints) {
  json ret;
  if (ordersConstraints.isCur1Defined()) {
    ret.emplace("cur1", ordersConstraints.curStr1());
  }
  if (ordersConstraints.isCur2Defined()) {
    ret.emplace("cur2", ordersConstraints.curStr2());
  }
  if (ordersConstraints.isPlacedTimeBeforeDefined()) {
    ret.emplace("placedBefore", ToString(ordersConstraints.placedBefore()));
  }
  if (ordersConstraints.isPlacedTimeAfterDefined()) {
    ret.emplace("placedAfter", ToString(ordersConstraints.placedAfter()));
  }
  if (ordersConstraints.isOrderIdDefined()) {
    json orderIds = json::array();
    for (const OrderId &orderId : ordersConstraints.orderIdSet()) {
      orderIds.emplace_back(orderId);
    }
    ret.emplace("matchIds", std::move(orderIds));
  }
  return ret;
}
}  // namespace

void QueryResultPrinter::printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange,
                                           const OrdersConstraints &ordersConstraints) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Account", "Exchange Id", "Placed time", "Side", "Price", "Matched Amount",
                              "Remaining Amount");
      for (const auto &[exchangePtr, openedOrders] : openedOrdersPerExchange) {
        for (const Order &openedOrder : openedOrders) {
          simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), openedOrder.id(),
                                   openedOrder.placedTimeStr(), openedOrder.sideStr(), openedOrder.price().str(),
                                   openedOrder.matchedVolume().str(), openedOrder.remainingVolume().str());
        }
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kOrdersOpened));
      json inOpt = OrdersConstraintsToJson(ordersConstraints);

      if (!inOpt.empty()) {
        in.emplace("opt", std::move(inOpt));
      }

      json out = json::object();
      for (const auto &[exchangePtr, openedOrders] : openedOrdersPerExchange) {
        json orders = json::array();
        for (const Order &openedOrder : openedOrders) {
          json &order = orders.emplace_back();
          order.emplace("id", openedOrder.id());
          order.emplace("pair", openedOrder.market().str());
          order.emplace("placedTime", openedOrder.placedTimeStr());
          order.emplace("side", openedOrder.sideStr());
          order.emplace("price", openedOrder.price().amountStr());
          order.emplace("matched", openedOrder.matchedVolume().amountStr());
          order.emplace("remaining", openedOrder.remainingVolume().amountStr());
        }

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json ordersPerExchangeUser;
          ordersPerExchangeUser.emplace(exchangePtr->keyName(), std::move(orders));
          out.emplace(exchangePtr->name(), std::move(ordersPerExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(orders));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printCancelledOrders(const NbCancelledOrdersPerExchange &nbCancelledOrdersPerExchange,
                                              const OrdersConstraints &ordersConstraints) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Account", "Number of cancelled orders");
      for (const auto &[exchangePtr, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
        simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), nbCancelledOrders);
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kOrdersCancel));
      json inOpt = OrdersConstraintsToJson(ordersConstraints);

      if (!inOpt.empty()) {
        in.emplace("opt", std::move(inOpt));
      }

      json out = json::object();
      for (const auto &[exchangePtr, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
        json cancelledOrdersForAccount;
        cancelledOrdersForAccount.emplace("nb", nbCancelledOrders);

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json cancelledOrdersForExchangeUser;
          cancelledOrdersForExchangeUser.emplace(exchangePtr->keyName(), std::move(cancelledOrdersForAccount));
          out.emplace(exchangePtr->name(), std::move(cancelledOrdersForExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(cancelledOrdersForAccount));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

namespace {

enum class DepositOrWithdrawEnum : int8_t { kDeposit, kWithdraw };

json DepositsConstraintsToJson(const WithdrawsOrDepositsConstraints &constraints,
                               DepositOrWithdrawEnum depositOrWithdraw) {
  json ret;
  if (constraints.isCurDefined()) {
    ret.emplace("cur", constraints.currencyCode().str());
  }
  if (constraints.isTimeBeforeDefined()) {
    ret.emplace(depositOrWithdraw == DepositOrWithdrawEnum::kDeposit ? "receivedBefore" : "sentBefore",
                ToString(constraints.timeBefore()));
  }
  if (constraints.isTimeAfterDefined()) {
    ret.emplace(depositOrWithdraw == DepositOrWithdrawEnum::kDeposit ? "receivedAfter" : "sentAfter",
                ToString(constraints.timeAfter()));
  }
  if (constraints.isIdDefined()) {
    json depositIds = json::array();
    for (const string &depositId : constraints.idSet()) {
      depositIds.emplace_back(depositId);
    }
    ret.emplace("matchIds", std::move(depositIds));
  }
  return ret;
}
}  // namespace

void QueryResultPrinter::printRecentDeposits(const DepositsPerExchange &depositsPerExchange,
                                             const DepositsConstraints &depositsConstraints) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Account", "Exchange Id", "Received time", "Amount", "Status");
      for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
        for (const Deposit &deposit : deposits) {
          simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), deposit.id(), deposit.timeStr(),
                                   deposit.amount().str(), deposit.statusStr());
        }
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kRecentDeposits));
      json inOpt = DepositsConstraintsToJson(depositsConstraints, DepositOrWithdrawEnum::kDeposit);

      if (!inOpt.empty()) {
        in.emplace("opt", std::move(inOpt));
      }

      json out = json::object();
      for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
        json depositsJson = json::array();
        for (const Deposit &deposit : deposits) {
          json &depositJson = depositsJson.emplace_back();
          depositJson.emplace("id", deposit.id());
          depositJson.emplace("cur", deposit.amount().currencyStr());
          depositJson.emplace("receivedTime", deposit.timeStr());
          depositJson.emplace("amount", deposit.amount().amountStr());
          depositJson.emplace("status", deposit.statusStr());
        }

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json depositsPerExchangeUser;
          depositsPerExchangeUser.emplace(exchangePtr->keyName(), std::move(depositsJson));
          out.emplace(exchangePtr->name(), std::move(depositsPerExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(depositsJson));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printRecentWithdraws(const WithdrawsPerExchange &withdrawsPerExchange,
                                              const WithdrawsConstraints &withdrawsConstraints) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Account", "Exchange Id", "Sent time", "Net Emitted Amount", "Fee", "Status");
      for (const auto &[exchangePtr, withdraws] : withdrawsPerExchange) {
        for (const Withdraw &withdraw : withdraws) {
          simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), withdraw.id(), withdraw.timeStr(),
                                   withdraw.amount().str(), withdraw.withdrawFee().str(), withdraw.statusStr());
        }
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kRecentWithdraws));
      json inOpt = DepositsConstraintsToJson(withdrawsConstraints, DepositOrWithdrawEnum::kWithdraw);

      if (!inOpt.empty()) {
        in.emplace("opt", std::move(inOpt));
      }

      json out = json::object();
      for (const auto &[exchangePtr, withdraws] : withdrawsPerExchange) {
        json withdrawsJson = json::array();
        for (const Withdraw &withdraw : withdraws) {
          json &withdrawJson = withdrawsJson.emplace_back();
          withdrawJson.emplace("id", withdraw.id());
          withdrawJson.emplace("cur", withdraw.amount().currencyStr());
          withdrawJson.emplace("sentTime", withdraw.timeStr());
          withdrawJson.emplace("netEmittedAmount", withdraw.amount().amountStr());
          withdrawJson.emplace("fee", withdraw.withdrawFee().amountStr());
          withdrawJson.emplace("status", withdraw.statusStr());
        }

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json withdrawsPerExchangeUser;
          withdrawsPerExchangeUser.emplace(exchangePtr->keyName(), std::move(withdrawsJson));
          out.emplace(exchangePtr->name(), std::move(withdrawsPerExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(withdrawsJson));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printConversionPath(Market mk,
                                             const ConversionPathPerExchange &conversionPathsPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string conversionPathStrHeader("Fastest conversion path for ");
      conversionPathStrHeader.append(mk.str());
      SimpleTable simpleTable("Exchange", std::move(conversionPathStrHeader));
      for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
        if (!conversionPath.empty()) {
          string conversionPathStr;
          for (Market market : conversionPath) {
            if (!conversionPathStr.empty()) {
              conversionPathStr.push_back(',');
            }
            conversionPathStr.append(market.str());
          }
          simpleTable.emplace_back(e->name(), std::move(conversionPathStr));
        }
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kConversionPath));
      json inOpt;
      inOpt.emplace("market", mk.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
        if (!conversionPath.empty()) {
          json conversionPathForExchange;
          for (Market market : conversionPath) {
            conversionPathForExchange.emplace_back(market.str());
          }
          out.emplace(e->name(), std::move(conversionPathForExchange));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printWithdrawFees(const MonetaryAmountPerExchange &withdrawFeePerExchange,
                                           CurrencyCode cur) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Withdraw fee");
      for (const auto &[e, withdrawFee] : withdrawFeePerExchange) {
        simpleTable.emplace_back(e->name(), withdrawFee.str());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kWithdrawFee));
      json inOpt;
      inOpt.emplace("cur", cur.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[e, withdrawFee] : withdrawFeePerExchange) {
        out.emplace(e->name(), withdrawFee.amountStr());
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printLast24hTradedVolume(Market mk,
                                                  const MonetaryAmountPerExchange &tradedVolumePerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string headerTradedVolume("Last 24h ");
      headerTradedVolume.append(mk.str());
      headerTradedVolume.append(" traded volume");
      SimpleTable simpleTable("Exchange", std::move(headerTradedVolume));
      for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
        simpleTable.emplace_back(e->name(), tradedVolume.str());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLast24hTradedVolume));
      json inOpt;
      inOpt.emplace("market", mk.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
        out.emplace(e->name(), tradedVolume.amountStr());
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printLastTrades(Market mk, int nbLastTrades,
                                         const LastTradesPerExchange &lastTradesPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
        string buyTitle = mk.base().str();
        string sellTitle = buyTitle;
        buyTitle.append(" buys");
        sellTitle.append(" sells");
        string priceTitle("Price in ");
        mk.quote().appendStrTo(priceTitle);

        string title(exchangePtr->name());
        title.append(" trades - UTC");

        SimpleTable simpleTable(std::move(title), std::move(buyTitle), std::move(priceTitle), std::move(sellTitle));
        std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, mk.base()), MonetaryAmount(0, mk.base())};
        MonetaryAmount totalPrice(0, mk.quote());
        std::array<int, 2> nb{};
        for (const PublicTrade &trade : lastTrades) {
          if (trade.side() == TradeSide::kBuy) {
            simpleTable.emplace_back(trade.timeStr(), trade.amount().amountStr(), trade.price().amountStr(), "");
            totalAmounts[0] += trade.amount();
            ++nb[0];
          } else {
            simpleTable.emplace_back(trade.timeStr(), "", trade.price().amountStr(), trade.amount().amountStr());
            totalAmounts[1] += trade.amount();
            ++nb[1];
          }
          totalPrice += trade.price();
        }
        if (nb[0] + nb[1] > 0) {
          simpleTable.push_back(SimpleTable::Row::kDivider);
          std::array<string, 2> summary;
          for (int buyOrSell = 0; buyOrSell < 2; ++buyOrSell) {
            summary[buyOrSell].append(totalAmounts[buyOrSell].str());
            summary[buyOrSell].append(" (");
            AppendString(summary[buyOrSell], nb[buyOrSell]);
            summary[buyOrSell].push_back(' ');
            summary[buyOrSell].append(buyOrSell == 0 ? "buys" : "sells");
            summary[buyOrSell].push_back(')');
          }

          MonetaryAmount avgPrice = totalPrice / (nb[0] + nb[1]);
          simpleTable.emplace_back("Summary", std::move(summary[0]), avgPrice.str(), std::move(summary[1]));
        }

        printTable(simpleTable);
      }
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLastTrades));
      json inOpt;
      inOpt.emplace("market", mk.str());
      inOpt.emplace("nb", nbLastTrades);
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
        json lastTradesJson = json::array();
        for (const PublicTrade &trade : lastTrades) {
          json &lastTrade = lastTradesJson.emplace_back();
          lastTrade.emplace("a", trade.amount().amountStr());
          lastTrade.emplace("p", trade.price().amountStr());
          lastTrade.emplace("time", trade.timeStr());
          lastTrade.emplace("side", SideStr(trade.side()));
        }
        out.emplace(exchangePtr->name(), std::move(lastTradesJson));
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printLastPrice(Market mk, const MonetaryAmountPerExchange &pricePerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string headerLastPrice(mk.str());
      headerLastPrice.append(" last price");
      SimpleTable simpleTable("Exchange", std::move(headerLastPrice));
      for (const auto &[e, lastPrice] : pricePerExchange) {
        simpleTable.emplace_back(e->name(), lastPrice.str());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLastPrice));
      json inOpt;
      inOpt.emplace("market", mk.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[e, lastPrice] : pricePerExchange) {
        out.emplace(e->name(), lastPrice.amountStr());
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printWithdraw(const DeliveredWithdrawInfoWithExchanges &deliveredWithdrawInfoWithExchanges,
                                       bool isPercentageWithdraw, const WithdrawOptions &withdrawOptions) const {
  const DeliveredWithdrawInfo &deliveredWithdrawInfo = deliveredWithdrawInfoWithExchanges.second;
  MonetaryAmount grossAmount = deliveredWithdrawInfo.grossAmount();
  const Exchange &fromExchange = *deliveredWithdrawInfoWithExchanges.first.front();
  const Exchange &toExchange = *deliveredWithdrawInfoWithExchanges.first.back();
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("From Exchange", "From Account", "Gross withdraw amount", "Initiated time", "To Exchange",
                              "To Account", "Net received amount", "Received time");
      simpleTable.emplace_back(fromExchange.name(), fromExchange.keyName(), grossAmount.str(),
                               ToString(deliveredWithdrawInfo.initiatedTime()), toExchange.name(), toExchange.keyName(),
                               deliveredWithdrawInfo.receivedAmount().str(),
                               ToString(deliveredWithdrawInfo.receivedTime()));
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kWithdraw));
      json inOpt;
      inOpt.emplace("cur", grossAmount.currencyStr());
      inOpt.emplace("grossAmount", grossAmount.amountStr());
      inOpt.emplace("isPercentage", isPercentageWithdraw);
      inOpt.emplace("syncPolicy", withdrawOptions.withdrawSyncPolicyStr());
      in.emplace("opt", std::move(inOpt));

      json from;
      from.emplace("exchange", fromExchange.name());
      from.emplace("account", fromExchange.keyName());

      json to;
      to.emplace("exchange", toExchange.name());
      to.emplace("account", toExchange.keyName());
      to.emplace("address", deliveredWithdrawInfo.receivingWallet().address());
      if (deliveredWithdrawInfo.receivingWallet().hasTag()) {
        to.emplace("tag", deliveredWithdrawInfo.receivingWallet().tag());
      }

      json out;
      out.emplace("from", std::move(from));
      out.emplace("to", std::move(to));
      out.emplace("initiatedTime", ToString(deliveredWithdrawInfo.initiatedTime()));
      out.emplace("receivedTime", ToString(deliveredWithdrawInfo.receivedTime()));
      out.emplace("netReceivedAmount", deliveredWithdrawInfo.receivedAmount().amountStr());

      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printDustSweeper(
    const TradedAmountsVectorWithFinalAmountPerExchange &tradedAmountsVectorWithFinalAmountPerExchange,
    CurrencyCode currencyCode) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable simpleTable("Exchange", "Account", "Trades", "Final Amount");
      for (const auto &[exchangePtr, tradedAmountsVectorWithFinalAmount] :
           tradedAmountsVectorWithFinalAmountPerExchange) {
        string tradesStr;
        for (const auto &tradedAmounts : tradedAmountsVectorWithFinalAmount.tradedAmountsVector) {
          if (!tradesStr.empty()) {
            tradesStr.append(", ");
          }
          tradesStr.append(tradedAmounts.str());
        }
        simpleTable.emplace_back(exchangePtr->name(), exchangePtr->keyName(), std::move(tradesStr),
                                 tradedAmountsVectorWithFinalAmount.finalAmount.str());
      }
      printTable(simpleTable);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kDustSweeper));
      json inOpt;
      inOpt.emplace("cur", currencyCode.str());
      in.emplace("opt", std::move(inOpt));

      json out = json::object();
      for (const auto &[exchangePtr, tradedAmountsVectorWithFinalAmount] :
           tradedAmountsVectorWithFinalAmountPerExchange) {
        json tradedAmountsArray = json::array_t();
        for (const auto &tradedAmounts : tradedAmountsVectorWithFinalAmount.tradedAmountsVector) {
          json tradedAmountsData;
          tradedAmountsData.emplace("from", tradedAmounts.tradedFrom.str());
          tradedAmountsData.emplace("to", tradedAmounts.tradedTo.str());
          tradedAmountsArray.push_back(std::move(tradedAmountsData));
        }

        json tradedInfoPerExchangeData;
        tradedInfoPerExchangeData.emplace("trades", std::move(tradedAmountsArray));
        tradedInfoPerExchangeData.emplace("finalAmount", tradedAmountsVectorWithFinalAmount.finalAmount.str());

        auto it = out.find(exchangePtr->name());
        if (it == out.end()) {
          json dataForExchangeUser;
          dataForExchangeUser.emplace(exchangePtr->keyName(), std::move(tradedInfoPerExchangeData));
          out.emplace(exchangePtr->name(), std::move(dataForExchangeUser));
        } else {
          it->emplace(exchangePtr->keyName(), std::move(tradedInfoPerExchangeData));
        }
      }
      printJson(std::move(in), std::move(out));
      break;
    }
    case ApiOutputType::kNoPrint:
      break;
  }
}

void QueryResultPrinter::printTable(const SimpleTable &simpleTable) const {
  std::ostringstream ss;
  std::ostream &os = _pOs != nullptr ? *_pOs : ss;
  simpleTable.print(os);

  if (_pOs != nullptr) {
    *_pOs << std::endl;
  } else {
    // logger library automatically adds a newline as suffix
    _outputLogger->info(ss.view());
  }
}

void QueryResultPrinter::printJson(json &&in, json &&out) const {
  json ret;
  ret.emplace("in", std::move(in));
  ret.emplace("out", std::move(out));

  if (_pOs != nullptr) {
    *_pOs << ret.dump() << std::endl;
  } else {
    _outputLogger->info(ret.dump());
  }
}

}  // namespace cct