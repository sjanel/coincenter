#include "exchangesorchestrator.hpp"

#include <algorithm>
#include <array>
#include <execution>

#include "cct_log.hpp"
#include "cct_smallvector.hpp"

namespace cct {

namespace {

template <class MainVec>
void FilterVector(MainVec &main, std::span<const bool> considerSpan) {
  // Erases from last to first to keep index consistent
  for (auto pos = main.size(); pos > 0; --pos) {
    if (!considerSpan[pos - 1]) {
      main.erase(main.begin() + pos - 1);
    }
  }
}

using ExchangeAmountPair = std::pair<Exchange *, MonetaryAmount>;
using ExchangeAmountPairVector = SmallVector<ExchangeAmountPair, kTypicalNbPrivateAccounts>;
using ExchangeAmountToCurrency = std::tuple<Exchange *, MonetaryAmount, CurrencyCode>;
using ExchangeAmountToCurrencyToAmount = std::tuple<Exchange *, MonetaryAmount, CurrencyCode, MonetaryAmount>;
using ExchangeAmountToCurrencyVector = SmallVector<ExchangeAmountToCurrency, kTypicalNbPrivateAccounts>;
using ExchangeAmountToCurrencyToAmountVector = SmallVector<ExchangeAmountToCurrencyToAmount, kTypicalNbPrivateAccounts>;
using TradedAmountsVector = ExchangesOrchestrator::TradedAmountsVector;

template <class FilterFunc>
void Filter(ExchangeAmountPairVector &exchangeAmountPairVector, FilterFunc &func) {
  SmallVector<bool, kTypicalNbPrivateAccounts> keepExchanges(exchangeAmountPairVector.size());
  std::transform(std::execution::par, exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(),
                 keepExchanges.begin(), func);
  FilterVector(exchangeAmountPairVector, keepExchanges);
}

void FilterConversionPaths(ExchangeAmountPairVector &exchangeAmountPairVector, CurrencyCode fromCurrency,
                           CurrencyCode toCurrency) {
  auto filterFunc = [fromCurrency, toCurrency](auto &exchangeAmountPair) {
    return !exchangeAmountPair.second.isZero() &&
           !exchangeAmountPair.first->apiPublic().findMarketsPath(fromCurrency, toCurrency).empty();
  };
  Filter(exchangeAmountPairVector, filterFunc);
}

void FilterMarkets(ExchangeAmountPairVector &exchangeAmountPairVector, CurrencyCode fromCurrency,
                   CurrencyCode toCurrency) {
  Market m(fromCurrency, toCurrency);
  auto filterFunc = [m](auto &exchangeAmountPair) {
    if (exchangeAmountPair.second.isZero()) {
      return false;
    }
    api::ExchangePublic::MarketSet markets = exchangeAmountPair.first->apiPublic().queryTradableMarkets();
    return markets.contains(m) || markets.contains(m.reverse());
  };
  Filter(exchangeAmountPairVector, filterFunc);
}

ExchangeAmountPairVector ComputeExchangeAmountPairVector(CurrencyCode fromCurrency,
                                                         const BalancePerExchange &balancePerExchange) {
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector;
  std::transform(balancePerExchange.begin(), balancePerExchange.end(), std::back_inserter(exchangeAmountPairVector),
                 [fromCurrency](auto &exchangeBalancePair) {
                   return std::make_pair(exchangeBalancePair.first, exchangeBalancePair.second.get(fromCurrency));
                 });
  return exchangeAmountPairVector;
}

TradedAmounts LaunchAndCollectTrades(ExchangeAmountPairVector::iterator first, ExchangeAmountPairVector::iterator last,
                                     CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                     const TradeOptions &tradeOptions) {
  SmallVector<TradedAmounts, kTypicalNbPrivateAccounts> tradeAmountsPerExchange(std::distance(first, last));
  std::transform(std::execution::par, first, last, tradeAmountsPerExchange.begin(),
                 [toCurrency, &tradeOptions](auto &exchangeBalancePair) {
                   return exchangeBalancePair.first->apiPrivate().trade(exchangeBalancePair.second, toCurrency,
                                                                        tradeOptions);
                 });
  return std::accumulate(tradeAmountsPerExchange.begin(), tradeAmountsPerExchange.end(),
                         TradedAmounts(fromCurrency, toCurrency));
}

template <class Iterator>
TradedAmountsVector LaunchAndCollectTrades(Iterator first, Iterator last, const TradeOptions &tradeOptions) {
  TradedAmountsVector tradeAmountsPerExchange(std::distance(first, last));
  std::transform(std::execution::par, first, last, tradeAmountsPerExchange.begin(), [&tradeOptions](auto &t) {
    return std::get<0>(t)->apiPrivate().trade(std::get<1>(t), std::get<2>(t), tradeOptions);
  });
  return tradeAmountsPerExchange;
}

}  // namespace

ExchangeTickerMaps ExchangesOrchestrator::getTickerInformation(ExchangeNameSpan exchangeNames) {
  log::info("Ticker information for {}", ConstructAccumulatedExchangeNames(exchangeNames));

  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeTickerMaps ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [](Exchange *e) { return std::make_pair(e, e->queryAllApproximatedOrderBooks(1)); });

  return ret;
}

MarketOrderBookConversionRates ExchangesOrchestrator::getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                                          CurrencyCode equiCurrencyCode,
                                                                          std::optional<int> depth) {
  log::info("Order book of {} on {} requested{}{}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames),
            equiCurrencyCode.isNeutral() ? "" : " with equi currency ",
            equiCurrencyCode.isNeutral() ? "" : equiCurrencyCode.str());
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradable;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), isMarketTradable.begin(),
                 [m](Exchange *e) { return e->queryTradableMarkets().contains(m); });

  FilterVector(selectedExchanges, isMarketTradable);

  MarketOrderBookConversionRates ret(selectedExchanges.size());
  auto marketOrderBooksFunc = [m, equiCurrencyCode, depth](Exchange *e) {
    std::optional<MonetaryAmount> optConversionRate =
        equiCurrencyCode.isNeutral() ? std::nullopt
                                     : e->apiPublic().convert(MonetaryAmount(1, m.quote()), equiCurrencyCode);
    MarketOrderBook marketOrderBook(depth ? e->queryOrderBook(m, *depth) : e->queryOrderBook(m));
    if (!optConversionRate && !equiCurrencyCode.isNeutral()) {
      log::warn("Unable to convert {} into {} on {}", marketOrderBook.market().quoteStr(), equiCurrencyCode.str(),
                e->name());
    }
    return std::make_tuple(e->name(), std::move(marketOrderBook), optConversionRate);
  };
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 marketOrderBooksFunc);
  return ret;
}

BalancePerExchange ExchangesOrchestrator::getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                                     CurrencyCode equiCurrency) {
  log::info("Query balance from {}{}{}", ConstructAccumulatedExchangeNames(privateExchangeNames),
            equiCurrency.isNeutral() ? "" : " with equi currency ", equiCurrency.str());

  ExchangeRetriever::SelectedExchanges balanceExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts> balancePortfolios(balanceExchanges.size());
  std::transform(std::execution::par, balanceExchanges.begin(), balanceExchanges.end(), balancePortfolios.begin(),
                 [equiCurrency](Exchange *e) { return e->apiPrivate().getAccountBalance(equiCurrency); });

  BalancePerExchange ret;
  ret.reserve(balanceExchanges.size());
  std::transform(balanceExchanges.begin(), balanceExchanges.end(), std::make_move_iterator(balancePortfolios.begin()),
                 std::back_inserter(ret),
                 [](Exchange *e, BalancePortfolio &&b) { return std::make_pair(e, std::move(b)); });

  return ret;
}

WalletPerExchange ExchangesOrchestrator::getDepositInfo(std::span<const PrivateExchangeName> privateExchangeNames,
                                                        CurrencyCode depositCurrency) {
  log::info("Query {} deposit information from {}", depositCurrency.str(),
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges depositInfoExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  /// Filter only on exchanges with can receive given currency
  SmallVector<bool, kTypicalNbPrivateAccounts> canDepositCurrency(depositInfoExchanges.size());
  // Do not call in parallel here because tradable currencies service could be queried from several identical public
  // exchanges (when there are several accounts for one exchange)
  std::transform(depositInfoExchanges.begin(), depositInfoExchanges.end(), canDepositCurrency.begin(),
                 [depositCurrency](Exchange *e) {
                   auto tradableCur = e->queryTradableCurrencies();
                   auto curIt = tradableCur.find(depositCurrency);
                   if (curIt == tradableCur.end()) {
                     return false;
                   }
                   if (curIt->canDeposit()) {
                     log::debug("{} can be deposited on {} currently", curIt->standardCode().str(), e->name());
                     return true;
                   } else {
                     log::info("{} cannot be deposited on {} currently", curIt->standardCode().str(), e->name());
                     return false;
                   }
                 });

  FilterVector(depositInfoExchanges, canDepositCurrency);

  SmallVector<Wallet, kTypicalNbPrivateAccounts> walletPerExchange(depositInfoExchanges.size());
  std::transform(std::execution::par, depositInfoExchanges.begin(), depositInfoExchanges.end(),
                 walletPerExchange.begin(),
                 [depositCurrency](Exchange *e) { return e->apiPrivate().queryDepositWallet(depositCurrency); });
  WalletPerExchange ret;
  ret.reserve(depositInfoExchanges.size());
  std::transform(depositInfoExchanges.begin(), depositInfoExchanges.end(),
                 std::make_move_iterator(walletPerExchange.begin()), std::back_inserter(ret),
                 [](const Exchange *e, Wallet &&w) { return std::make_pair(e, std::move(w)); });
  return ret;
}

OpenedOrdersPerExchange ExchangesOrchestrator::getOpenedOrders(
    std::span<const PrivateExchangeName> privateExchangeNames, const OrdersConstraints &openedOrdersConstraints) {
  log::info("Query opened orders matching {} on {}", openedOrdersConstraints.str(),
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  OpenedOrdersPerExchange ret(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
      [&](Exchange *e) { return std::make_pair(e, e->apiPrivate().queryOpenedOrders(openedOrdersConstraints)); });

  return ret;
}

void ExchangesOrchestrator::cancelOrders(std::span<const PrivateExchangeName> privateExchangeNames,
                                         const OrdersConstraints &ordersConstraints) {
  log::info("Cancel opened orders matching {} on {}", ordersConstraints.str(),
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  std::for_each(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                [&](Exchange *e) { e->apiPrivate().cancelOpenedOrders(ordersConstraints); });
}

ConversionPathPerExchange ExchangesOrchestrator::getConversionPaths(Market m, ExchangeNameSpan exchangeNames) {
  log::info("Query {} conversion path from {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  ConversionPathPerExchange conversionPathPerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 conversionPathPerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->apiPublic().findMarketsPath(m.base(), m.quote())); });

  return conversionPathPerExchange;
}

MarketsPerExchange ExchangesOrchestrator::getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2,
                                                                ExchangeNameSpan exchangeNames) {
  string curStr(cur1.str());
  if (!cur2.isNeutral()) {
    curStr.push_back('-');
    curStr.append(cur2.str());
  }
  log::info("Query markets with {} from {}", curStr, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  MarketsPerExchange marketsPerExchange(selectedExchanges.size());
  auto marketsWithCur = [cur1, cur2](Exchange *e) {
    api::ExchangePublic::MarketSet markets = e->queryTradableMarkets();
    api::ExchangePublic::MarketSet ret;
    std::copy_if(markets.begin(), markets.end(), std::inserter(ret, ret.end()),
                 [cur1, cur2](Market m) { return m.canTrade(cur1) && (cur2.isNeutral() || m.canTrade(cur2)); });
    return std::make_pair(e, std::move(ret));
  };
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), marketsPerExchange.begin(),
                 marketsWithCur);
  return marketsPerExchange;
}

UniquePublicSelectedExchanges ExchangesOrchestrator::getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                                                 ExchangeNameSpan exchangeNames,
                                                                                 bool shouldBeWithdrawable) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isCurrencyTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isCurrencyTradablePerExchange.begin(), [currencyCode, shouldBeWithdrawable](Exchange *e) {
                   CurrencyExchangeFlatSet currencies = e->queryTradableCurrencies();
                   auto foundIt = currencies.find(currencyCode);
                   return foundIt != currencies.end() && (!shouldBeWithdrawable || foundIt->canWithdraw());
                 });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isCurrencyTradablePerExchange);
  return selectedExchanges;
}

UniquePublicSelectedExchanges ExchangesOrchestrator::getExchangesTradingMarket(Market m,
                                                                               ExchangeNameSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isMarketTradablePerExchange.begin(),
                 [m](Exchange *e) { return e->queryTradableMarkets().contains(m); });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isMarketTradablePerExchange);

  return selectedExchanges;
}

TradedAmounts ExchangesOrchestrator::trade(MonetaryAmount startAmount, bool isPercentageTrade, CurrencyCode toCurrency,
                                           std::span<const PrivateExchangeName> privateExchangeNames,
                                           const TradeOptions &tradeOptions) {
  if (privateExchangeNames.size() == 1 && !isPercentageTrade) {
    // In this special case we don't need to call the balance - call trade directly
    Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeNames.front());
    return exchange.apiPrivate().trade(startAmount, toCurrency, tradeOptions);
  }

  const CurrencyCode fromCurrency = startAmount.currencyCode();

  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector =
      ComputeExchangeAmountPairVector(fromCurrency, getBalance(privateExchangeNames));

  if (tradeOptions.isMultiTradeAllowed()) {
    FilterConversionPaths(exchangeAmountPairVector, fromCurrency, toCurrency);
  } else {
    FilterMarkets(exchangeAmountPairVector, fromCurrency, toCurrency);
  }

  // Sort exchanges from largest to lowest available amount
  std::ranges::sort(exchangeAmountPairVector, [](const ExchangeAmountPair &lhs, const ExchangeAmountPair &rhs) {
    return lhs.second > rhs.second;
  });

  // Locate the point where there is enough available amount to trade for this currency
  MonetaryAmount currentTotalAmount(0, fromCurrency);

  if (isPercentageTrade) {
    MonetaryAmount totalAvailableAmount =
        std::accumulate(exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(), currentTotalAmount,
                        [](MonetaryAmount tot, const auto &p) { return tot + p.second; });
    startAmount = (totalAvailableAmount * startAmount.toNeutral()) / 100;
  }
  auto it = exchangeAmountPairVector.begin();
  for (auto endIt = exchangeAmountPairVector.end(); it != endIt && currentTotalAmount < startAmount; ++it) {
    if (currentTotalAmount + it->second > startAmount) {
      // Cap last amount such that total start trade on all exchanges reaches exactly 'startAmount'
      it->second = startAmount - currentTotalAmount;
    }
    currentTotalAmount += it->second;
  }

  if (currentTotalAmount < startAmount) {
    log::warn("Will trade {} < {} amount", currentTotalAmount.str(), startAmount.str());
  }

  /// We have enough total available amount. Launch all trades in parallel
  return LaunchAndCollectTrades(exchangeAmountPairVector.begin(), it, fromCurrency, toCurrency, tradeOptions);
}

TradedAmounts ExchangesOrchestrator::tradeAll(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                              std::span<const PrivateExchangeName> privateExchangeNames,
                                              const TradeOptions &tradeOptions) {
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector =
      ComputeExchangeAmountPairVector(fromCurrency, getBalance(privateExchangeNames));

  if (!tradeOptions.isMultiTradeAllowed()) {
    FilterMarkets(exchangeAmountPairVector, fromCurrency, toCurrency);
  }

  return LaunchAndCollectTrades(exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(), fromCurrency,
                                toCurrency, tradeOptions);
}

WithdrawInfo ExchangesOrchestrator::withdraw(MonetaryAmount grossAmount,
                                             const PrivateExchangeName &fromPrivateExchangeName,
                                             const PrivateExchangeName &toPrivateExchangeName) {
  log::info("Withdraw gross {} from {} to {} requested", grossAmount.str(), fromPrivateExchangeName.str(),
            toPrivateExchangeName.str());
  Exchange &fromExchange = _exchangeRetriever.retrieveUniqueCandidate(fromPrivateExchangeName);
  Exchange &toExchange = _exchangeRetriever.retrieveUniqueCandidate(toPrivateExchangeName);
  const std::array<Exchange *, 2> exchangePair = {std::addressof(fromExchange), std::addressof(toExchange)};
  std::array<CurrencyExchangeFlatSet, 2> currencyExchangeSets;
  std::transform(std::execution::par, exchangePair.begin(), exchangePair.end(), currencyExchangeSets.begin(),
                 [](Exchange *e) { return e->queryTradableCurrencies(); });

  const CurrencyCode currencyCode = grossAmount.currencyCode();
  if (!fromExchange.canWithdraw(currencyCode, currencyExchangeSets.front())) {
    string errMsg("It's currently not possible to withdraw ");
    errMsg.append(currencyCode.str()).append(" from ").append(fromPrivateExchangeName.str());
    throw exception(std::move(errMsg));
  }
  if (!toExchange.canDeposit(currencyCode, currencyExchangeSets.back())) {
    string errMsg("It's currently not possible to deposit ");
    errMsg.append(currencyCode.str()).append(" to ").append(fromPrivateExchangeName.str());
    throw exception(std::move(errMsg));
  }

  return fromExchange.apiPrivate().withdraw(grossAmount, toExchange.apiPrivate());
}

WithdrawFeePerExchange ExchangesOrchestrator::getWithdrawFees(CurrencyCode currencyCode,
                                                              ExchangeNameSpan exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames, true);

  WithdrawFeePerExchange withdrawFeePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 withdrawFeePerExchange.begin(),
                 [currencyCode](Exchange *e) { return std::make_pair(e, e->queryWithdrawalFee(currencyCode)); });
  return withdrawFeePerExchange;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLast24hTradedVolumePerExchange(Market m,
                                                                                   ExchangeNameSpan exchangeNames) {
  log::info("Query last 24h traded volume of {} pair on {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 tradedVolumePerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->queryLast24hVolume(m)); });
  return tradedVolumePerExchange;
}

LastTradesPerExchange ExchangesOrchestrator::getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames,
                                                                      int nbLastTrades) {
  log::info("Query {} last trades on {} volume from {}", nbLastTrades, m.str(),
            ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  LastTradesPerExchange ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [m, nbLastTrades](Exchange *e) {
                   return std::make_pair(static_cast<const Exchange *>(e), e->queryLastTrades(m, nbLastTrades));
                 });

  return ret;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames) {
  log::info("Query last price from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), lastPricePerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->queryLastPrice(m)); });
  return lastPricePerExchange;
}

}  // namespace cct
