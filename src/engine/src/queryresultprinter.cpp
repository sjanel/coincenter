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

namespace cct {
QueryResultPrinter::QueryResultPrinter(ApiOutputType apiOutputType)
    : _outputLogger(log::get(LoggingInfo::kOutputLoggerName)), _apiOutputType(apiOutputType) {}

QueryResultPrinter::QueryResultPrinter(std::ostream &os, ApiOutputType apiOutputType)
    : _pOs(&os), _outputLogger(log::get(LoggingInfo::kOutputLoggerName)), _apiOutputType(apiOutputType) {}

void QueryResultPrinter::printHealthCheck(const ExchangeHealthCheckStatus &healthCheckPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable t("Exchange", "Health Check status");
      for (const auto &[e, healthCheckValue] : healthCheckPerExchange) {
        t.emplace_back(e->name(), healthCheckValue ? "OK" : "Not OK!");
      }
      printTable(t);
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
      cur1.appendStr(marketsCol);
      if (!cur2.isNeutral()) {
        marketsCol.push_back('-');
        cur2.appendStr(marketsCol);
      }
      SimpleTable t("Exchange", std::move(marketsCol));
      for (const auto &[e, markets] : marketsPerExchange) {
        for (Market m : markets) {
          t.emplace_back(e->name(), m.str());
        }
      }
      printTable(t);
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
        for (const Market &m : markets) {
          marketsForExchange.emplace_back(m.str());
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
      SimpleTable t("Exchange", "Market", "Bid price", "Bid volume", "Ask price", "Ask volume");
      for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
        for (const auto &[m, marketOrderBook] : marketOrderBookMap) {
          t.emplace_back(e->name(), m.str(), marketOrderBook.highestBidPrice().str(),
                         marketOrderBook.amountAtBidPrice().str(), marketOrderBook.lowestAskPrice().str(),
                         marketOrderBook.amountAtAskPrice().str());
        }
        // Sort rows in lexicographical order for consistent output
        std::sort(t.begin(), t.end());
      }
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kTicker));

      json out = json::object();
      for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
        json allTickerForExchange;
        for (const auto &[m, marketOrderBook] : marketOrderBookMap) {
          json tickerForExchange;
          tickerForExchange.emplace("pair", m.str());
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
    Market m, CurrencyCode equiCurrencyCode, std::optional<int> depth,
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
      inOpt.emplace("pair", m.str());
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
      SimpleTable t("Exchange", "Account", std::move(walletStr), "Destination Tag");
      for (const auto &[exchangePtr, wallet] : walletPerExchange) {
        t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), wallet.address(), wallet.tag());
      }
      printTable(t);
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
      SimpleTable t("Exchange", "Account", std::move(tradedFromStr), std::move(tradedToStr));
      for (const auto &[exchangePtr, tradedAmount] : tradedAmountsPerExchange) {
        t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), tradedAmount.tradedFrom.str(),
                       tradedAmount.tradedTo.str());
      }
      printTable(t);
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
      SimpleTable t("Exchange", "Account", "Exchange Id", "Placed time", "Side", "Price", "Matched Amount",
                    "Remaining Amount");
      for (const auto &[exchangePtr, openedOrders] : openedOrdersPerExchange) {
        for (const Order &openedOrder : openedOrders) {
          t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), openedOrder.id(), openedOrder.placedTimeStr(),
                         openedOrder.sideStr(), openedOrder.price().str(), openedOrder.matchedVolume().str(),
                         openedOrder.remainingVolume().str());
        }
      }
      printTable(t);
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
      SimpleTable t("Exchange", "Account", "Number of cancelled orders");
      for (const auto &[exchangePtr, nbCancelledOrders] : nbCancelledOrdersPerExchange) {
        t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), nbCancelledOrders);
      }
      printTable(t);
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
json DepositsConstraintsToJson(const DepositsConstraints &depositsConstraints) {
  json ret;
  if (depositsConstraints.isCurDefined()) {
    ret.emplace("cur", depositsConstraints.currencyCode().str());
  }
  if (depositsConstraints.isReceivedTimeBeforeDefined()) {
    ret.emplace("receivedBefore", ToString(depositsConstraints.receivedBefore()));
  }
  if (depositsConstraints.isReceivedTimeAfterDefined()) {
    ret.emplace("receivedAfter", ToString(depositsConstraints.receivedAfter()));
  }
  if (depositsConstraints.isDepositIdDefined()) {
    json depositIds = json::array();
    for (const string &depositId : depositsConstraints.depositIdSet()) {
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
      SimpleTable t("Exchange", "Account", "Exchange Id", "Received time", "Amount");
      for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
        for (const Deposit &deposit : deposits) {
          t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), deposit.depositId(), deposit.receivedTimeStr(),
                         deposit.amount().str());
        }
      }
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kRecentDeposits));
      json inOpt = DepositsConstraintsToJson(depositsConstraints);

      if (!inOpt.empty()) {
        in.emplace("opt", std::move(inOpt));
      }

      json out = json::object();
      for (const auto &[exchangePtr, deposits] : depositsPerExchange) {
        json depositsJson = json::array();
        for (const Deposit &deposit : deposits) {
          json &depositJson = depositsJson.emplace_back();
          depositJson.emplace("id", deposit.depositId());
          depositJson.emplace("cur", deposit.amount().currencyStr());
          depositJson.emplace("receivedTime", deposit.receivedTimeStr());
          depositJson.emplace("amount", deposit.amount().amountStr());
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

void QueryResultPrinter::printConversionPath(Market m,
                                             const ConversionPathPerExchange &conversionPathsPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string conversionPathStrHeader("Fastest conversion path for ");
      conversionPathStrHeader.append(m.str());
      SimpleTable t("Exchange", std::move(conversionPathStrHeader));
      for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
        if (!conversionPath.empty()) {
          string conversionPathStr;
          for (Market market : conversionPath) {
            if (!conversionPathStr.empty()) {
              conversionPathStr.push_back(',');
            }
            conversionPathStr.append(market.str());
          }
          t.emplace_back(e->name(), std::move(conversionPathStr));
        }
      }
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kConversionPath));
      json inOpt;
      inOpt.emplace("market", m.str());
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
      SimpleTable t("Exchange", "Withdraw fee");
      for (const auto &[e, withdrawFee] : withdrawFeePerExchange) {
        t.emplace_back(e->name(), withdrawFee.str());
      }
      printTable(t);
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

void QueryResultPrinter::printLast24hTradedVolume(Market m,
                                                  const MonetaryAmountPerExchange &tradedVolumePerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string headerTradedVolume("Last 24h ");
      headerTradedVolume.append(m.str());
      headerTradedVolume.append(" traded volume");
      SimpleTable t("Exchange", std::move(headerTradedVolume));
      for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
        t.emplace_back(e->name(), tradedVolume.str());
      }
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLast24hTradedVolume));
      json inOpt;
      inOpt.emplace("market", m.str());
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

void QueryResultPrinter::printLastTrades(Market m, int nbLastTrades,
                                         const LastTradesPerExchange &lastTradesPerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
        string buyTitle = m.base().str();
        string sellTitle = buyTitle;
        buyTitle.append(" buys");
        sellTitle.append(" sells");
        string priceTitle("Price in ");
        m.quote().appendStr(priceTitle);

        string title(exchangePtr->name());
        title.append(" trades - UTC");

        SimpleTable t(std::move(title), std::move(buyTitle), std::move(priceTitle), std::move(sellTitle));
        std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, m.base()), MonetaryAmount(0, m.base())};
        MonetaryAmount totalPrice(0, m.quote());
        std::array<int, 2> nb{};
        for (const PublicTrade &trade : lastTrades) {
          if (trade.side() == TradeSide::kBuy) {
            t.emplace_back(trade.timeStr(), trade.amount().amountStr(), trade.price().amountStr(), "");
            totalAmounts[0] += trade.amount();
            ++nb[0];
          } else {
            t.emplace_back(trade.timeStr(), "", trade.price().amountStr(), trade.amount().amountStr());
            totalAmounts[1] += trade.amount();
            ++nb[1];
          }
          totalPrice += trade.price();
        }
        if (nb[0] + nb[1] > 0) {
          t.push_back(SimpleTable::Row::kDivider);
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
          t.emplace_back("Summary", std::move(summary[0]), avgPrice.str(), std::move(summary[1]));
        }

        printTable(t);
      }
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLastTrades));
      json inOpt;
      inOpt.emplace("market", m.str());
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

void QueryResultPrinter::printLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      string headerLastPrice(m.str());
      headerLastPrice.append(" last price");
      SimpleTable t("Exchange", std::move(headerLastPrice));
      for (const auto &[e, lastPrice] : pricePerExchange) {
        t.emplace_back(e->name(), lastPrice.str());
      }
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kLastPrice));
      json inOpt;
      inOpt.emplace("market", m.str());
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

void QueryResultPrinter::printWithdraw(const WithdrawInfo &withdrawInfo, MonetaryAmount grossAmount,
                                       bool isPercentageWithdraw, const ExchangeName &fromPrivateExchangeName,
                                       const ExchangeName &toPrivateExchangeName) const {
  switch (_apiOutputType) {
    case ApiOutputType::kFormattedTable: {
      SimpleTable t("From Exchange", "To Exchange", "Gross withdraw amount", "Initiated time", "Received time",
                    "Net received amount");
      t.emplace_back(fromPrivateExchangeName.name(), toPrivateExchangeName.name(), grossAmount.str(),
                     ToString(withdrawInfo.initiatedTime()), ToString(withdrawInfo.receivedTime()),
                     withdrawInfo.netEmittedAmount().str());
      printTable(t);
      break;
    }
    case ApiOutputType::kJson: {
      json in;
      in.emplace("req", CoincenterCommandTypeToString(CoincenterCommandType::kWithdraw));
      json inOpt;
      inOpt.emplace("cur", grossAmount.currencyStr());
      inOpt.emplace("grossAmount", grossAmount.amountStr());
      inOpt.emplace("isPercentage", isPercentageWithdraw);
      in.emplace("opt", std::move(inOpt));

      json from;
      from.emplace("exchange", fromPrivateExchangeName.name());
      from.emplace("account", fromPrivateExchangeName.keyName());

      json to;
      to.emplace("exchange", toPrivateExchangeName.name());
      to.emplace("account", toPrivateExchangeName.keyName());
      to.emplace("address", withdrawInfo.receivingWallet().address());
      if (withdrawInfo.receivingWallet().hasTag()) {
        to.emplace("tag", withdrawInfo.receivingWallet().tag());
      }

      json out;
      out.emplace("from", std::move(from));
      out.emplace("to", std::move(to));
      out.emplace("initiatedTime", ToString(withdrawInfo.initiatedTime()));
      out.emplace("receivedTime", ToString(withdrawInfo.receivedTime()));
      out.emplace("netReceivedAmount", withdrawInfo.netEmittedAmount().amountStr());

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
      SimpleTable t("Exchange", "Account", "Trades", "Final Amount");
      for (const auto &[exchangePtr, tradedAmountsVectorWithFinalAmount] :
           tradedAmountsVectorWithFinalAmountPerExchange) {
        string tradesStr;
        for (const auto &tradedAmounts : tradedAmountsVectorWithFinalAmount.tradedAmountsVector) {
          if (!tradesStr.empty()) {
            tradesStr.append(", ");
          }
          tradesStr.append(tradedAmounts.str());
        }
        t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), std::move(tradesStr),
                       tradedAmountsVectorWithFinalAmount.finalAmount.str());
      }
      printTable(t);
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

void QueryResultPrinter::printTable(const SimpleTable &t) const {
  std::ostringstream ss;
  std::ostream &os = _pOs ? *_pOs : ss;
  t.print(os);

  if (_pOs) {
    *_pOs << std::endl;
  } else {
    // logger library automatically adds a newline as suffix
#ifdef CCT_STRINGSTREAM_HAS_VIEW
    _outputLogger->info(ss.view());
#else
    _outputLogger->info(ss.str());
#endif
  }
}

void QueryResultPrinter::printJson(json &&in, json &&out) const {
  json ret;
  ret.emplace("in", std::move(in));
  ret.emplace("out", std::move(out));

  if (_pOs) {
    *_pOs << ret.dump() << std::endl;
  } else {
    _outputLogger->info(ret.dump());
  }
}

}  // namespace cct