#include "printqueryresults.hpp"

#include "balanceperexchangeportfolio.hpp"
#include "cct_string.hpp"
#include "simpletable.hpp"
#include "stringhelpers.hpp"

#define RETURN_IF_NO_PRINT \
  if (!_doPrint) return

namespace cct {
void QueryResultPrinter::printMarkets(CurrencyCode cur1, CurrencyCode cur2,
                                      const MarketsPerExchange &marketsPerExchange) const {
  RETURN_IF_NO_PRINT;
  string marketsCol("Markets with ");
  marketsCol.append(cur1.str());
  if (!cur2.isNeutral()) {
    marketsCol.push_back('-');
    marketsCol.append(cur2.str());
  }
  SimpleTable t("Exchange", std::move(marketsCol));
  for (const auto &[e, markets] : marketsPerExchange) {
    for (const Market &m : markets) {
      t.emplace_back(e->name(), m.str());
    }
  }
  t.print();
}

void QueryResultPrinter::printTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) const {
  RETURN_IF_NO_PRINT;
  SimpleTable t("Exchange", "Market", "Bid price", "Bid volume", "Ask price", "Ask volume");
  for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
    for (const auto &[m, marketOrderBook] : marketOrderBookMap) {
      t.emplace_back(e->name(), m.str(), marketOrderBook.highestBidPrice().str(),
                     marketOrderBook.amountAtBidPrice().str(), marketOrderBook.lowestAskPrice().str(),
                     marketOrderBook.amountAtAskPrice().str());
    }
  }
  t.print();
}

void QueryResultPrinter::printMarketOrderBooks(
    const MarketOrderBookConversionRates &marketOrderBooksConversionRates) const {
  RETURN_IF_NO_PRINT;
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
    marketOrderBook.print(std::cout, exchangeName, optConversionRate);
  }
}

void QueryResultPrinter::printBalance(const BalancePerExchange &balancePerExchange) const {
  RETURN_IF_NO_PRINT;
  BalancePerExchangePortfolio totalBalance;
  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    totalBalance.add(*exchangePtr, balancePortfolio);
  }
  totalBalance.print(std::cout, balancePerExchange.size() > 1);
}

void QueryResultPrinter::printDepositInfo(CurrencyCode depositCurrencyCode,
                                          const WalletPerExchange &walletPerExchange) const {
  RETURN_IF_NO_PRINT;
  string walletStr(depositCurrencyCode.str());
  walletStr.append(" address");
  SimpleTable t("Exchange", "Account", std::move(walletStr), "Destination Tag");
  for (const auto &[exchangePtr, wallet] : walletPerExchange) {
    t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), wallet.address(), wallet.tag());
  }
  t.print();
}

void QueryResultPrinter::printOpenedOrders(const OpenedOrdersPerExchange &openedOrdersPerExchange) const {
  RETURN_IF_NO_PRINT;
  SimpleTable t("Exchange", "Account", "Exchange Id", "Placed time", "Side", "Price", "Matched Amount",
                "Remaining Amount");
  for (const auto &[exchangePtr, openedOrders] : openedOrdersPerExchange) {
    for (const Order &openedOrder : openedOrders) {
      t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), openedOrder.id(), openedOrder.placedTimeStr(),
                     openedOrder.sideStr(), openedOrder.price().str(), openedOrder.matchedVolume().str(),
                     openedOrder.remainingVolume().str());
    }
  }
  t.print();
}

void QueryResultPrinter::printConversionPath(Market m,
                                             const ConversionPathPerExchange &conversionPathsPerExchange) const {
  RETURN_IF_NO_PRINT;
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
  t.print();
}

void QueryResultPrinter::printWithdrawFees(const WithdrawFeePerExchange &withdrawFeePerExchange) const {
  RETURN_IF_NO_PRINT;
  SimpleTable t("Exchange", "Withdraw fee");
  for (const auto &[e, withdrawFee] : withdrawFeePerExchange) {
    t.emplace_back(e->name(), withdrawFee.str());
  }
  t.print();
}

void QueryResultPrinter::printLast24hTradedVolume(Market m,
                                                  const MonetaryAmountPerExchange &tradedVolumePerExchange) const {
  RETURN_IF_NO_PRINT;
  string headerTradedVolume("Last 24h ");
  headerTradedVolume.append(m.str());
  headerTradedVolume.append(" traded volume");
  SimpleTable t("Exchange", std::move(headerTradedVolume));
  for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
    t.emplace_back(e->name(), tradedVolume.str());
  }
  t.print();
}

void QueryResultPrinter::printLastTrades(Market m, const LastTradesPerExchange &lastTradesPerExchange) const {
  RETURN_IF_NO_PRINT;
  for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
    string buyTitle(m.baseStr());
    buyTitle.append(" buys");
    string sellTitle(m.baseStr());
    sellTitle.append(" sells");
    string priceTitle("Price in ");
    priceTitle.append(m.quoteStr());

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

    t.print();
  }
}

void QueryResultPrinter::printLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange) const {
  RETURN_IF_NO_PRINT;
  string headerLastPrice(m.str());
  headerLastPrice.append(" last price");
  SimpleTable t("Exchange", std::move(headerLastPrice));
  for (const auto &[e, lastPrice] : pricePerExchange) {
    t.emplace_back(e->name(), lastPrice.str());
  }
  t.print();
}

}  // namespace cct