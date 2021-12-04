#include "printqueryresults.hpp"

#include "balanceperexchangeportfolio.hpp"
#include "cct_string.hpp"
#include "simpletable.hpp"
#include "stringhelpers.hpp"

namespace cct {
void PrintMarkets(CurrencyCode cur, const MarketsPerExchange &marketsPerExchange) {
  string marketsCol("Markets with ");
  marketsCol.append(cur.str());
  SimpleTable t("Exchange", std::move(marketsCol));
  for (const auto &[e, markets] : marketsPerExchange) {
    for (const Market &m : markets) {
      t.emplace_back(e->name(), m.str());
    }
  }
  t.print();
}

void PrintTickerInformation(const ExchangeTickerMaps &exchangeTickerMaps) {
  SimpleTable t("Exchange", "Market", "Bid price", "Bid volume", "Ask price", "Ask volume");
  for (const auto &[e, marketOrderBookMap] : exchangeTickerMaps) {
    for (const auto &[m, marketOrderBook] : marketOrderBookMap) {
      t.emplace_back(e->name(), m.assetsPairStr('-'), marketOrderBook.highestBidPrice().str(),
                     marketOrderBook.amountAtBidPrice().str(), marketOrderBook.lowestAskPrice().str(),
                     marketOrderBook.amountAtAskPrice().str());
    }
  }
  t.print();
}

void PrintMarketOrderBooks(const MarketOrderBookConversionRates &marketOrderBooksConversionRates) {
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
    marketOrderBook.print(std::cout, exchangeName, optConversionRate);
  }
}

void PrintBalance(const BalancePerExchange &balancePerExchange) {
  BalancePerExchangePortfolio totalBalance;
  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    totalBalance.add(*exchangePtr, balancePortfolio);
  }
  totalBalance.print(std::cout, balancePerExchange.size() > 1);
}

void PrintDepositInfo(CurrencyCode depositCurrencyCode, const WalletPerExchange &walletPerExchange) {
  string walletStr(depositCurrencyCode.str());
  walletStr.append(" address");
  SimpleTable t("Exchange", "Account", std::move(walletStr), "Destination Tag");
  for (const auto &[exchangePtr, wallet] : walletPerExchange) {
    t.emplace_back(exchangePtr->name(), exchangePtr->keyName(), wallet.address(), wallet.tag());
  }
  t.print();
}

void PrintConversionPath(Market m, const ConversionPathPerExchange &conversionPathsPerExchange) {
  string conversionPathStrHeader("Fastest conversion path for ");
  conversionPathStrHeader.append(m.assetsPairStr('-'));
  SimpleTable t("Exchange", std::move(conversionPathStrHeader));
  for (const auto &[e, conversionPath] : conversionPathsPerExchange) {
    if (!conversionPath.empty()) {
      string conversionPathStr;
      for (Market market : conversionPath) {
        if (!conversionPathStr.empty()) {
          conversionPathStr.push_back(',');
        }
        conversionPathStr.append(market.assetsPairStr('-'));
      }
      t.emplace_back(e->name(), std::move(conversionPathStr));
    }
  }
  t.print();
}

void PrintWithdrawFees(const WithdrawFeePerExchange &withdrawFeePerExchange) {
  SimpleTable t("Exchange", "Withdraw fee");
  for (const auto &[e, withdrawFee] : withdrawFeePerExchange) {
    t.emplace_back(e->name(), withdrawFee.str());
  }
  t.print();
}

void PrintLast24hTradedVolume(Market m, const MonetaryAmountPerExchange &tradedVolumePerExchange) {
  string headerTradedVolume("Last 24h ");
  headerTradedVolume.append(m.str());
  headerTradedVolume.append(" traded volume");
  SimpleTable t("Exchange", std::move(headerTradedVolume));
  for (const auto &[e, tradedVolume] : tradedVolumePerExchange) {
    t.emplace_back(e->name(), tradedVolume.str());
  }
  t.print();
}

void PrintLastTrades(Market m, const LastTradesPerExchange &lastTradesPerExchange) {
  for (const auto &[exchangePtr, lastTrades] : lastTradesPerExchange) {
    string buyTitle(m.base().str());
    buyTitle.append(" buys");
    string sellTitle(m.base().str());
    sellTitle.append(" sells");
    string priceTitle("Price in ");
    priceTitle.append(m.quote().str());

    string title(exchangePtr->name());
    title.append(" trades - UTC");

    SimpleTable t(std::move(title), std::move(buyTitle), std::move(priceTitle), std::move(sellTitle));
    std::array<MonetaryAmount, 2> totalAmounts{MonetaryAmount(0, m.base()), MonetaryAmount(0, m.base())};
    MonetaryAmount totalPrice(0, m.quote());
    std::array<int, 2> nb{};
    for (const PublicTrade &trade : lastTrades) {
      if (trade.type() == PublicTrade::Type::kBuy) {
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
        summary[buyOrSell].append(ToString<string>(nb[buyOrSell]));
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

void PrintLastPrice(Market m, const MonetaryAmountPerExchange &pricePerExchange) {
  string headerLastPrice(m.str());
  headerLastPrice.append(" last price");
  SimpleTable t("Exchange", std::move(headerLastPrice));
  for (const auto &[e, lastPrice] : pricePerExchange) {
    t.emplace_back(e->name(), lastPrice.str());
  }
  t.print();
}

}  // namespace cct