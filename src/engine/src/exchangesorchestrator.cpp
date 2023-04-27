#include "exchangesorchestrator.hpp"

#include <algorithm>
#include <array>
#include <execution>
#include <iterator>

#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "exchangepublicapitypes.hpp"

namespace cct {

using UniquePublicSelectedExchanges = ExchangeRetriever::UniquePublicSelectedExchanges;

namespace {

template <class MainVec>
void FilterVector(MainVec &main, std::span<const bool> considerSpan) {
  const auto begIt = main.begin();
  const auto endIt = main.end();
  main.erase(std::remove_if(begIt, endIt, [=](const auto &val) { return !considerSpan[&val - &*begIt]; }), endIt);
}

using ExchangeAmountPairVector = SmallVector<std::pair<Exchange *, MonetaryAmount>, kTypicalNbPrivateAccounts>;
using ExchangeAmountMarketsPathVector =
    SmallVector<std::tuple<Exchange *, MonetaryAmount, MarketsPath>, kTypicalNbPrivateAccounts>;
using ExchangeAmountToCurrency = std::tuple<Exchange *, MonetaryAmount, CurrencyCode, MarketsPath>;
using ExchangeAmountToCurrencyToAmount =
    std::tuple<Exchange *, MonetaryAmount, CurrencyCode, MarketsPath, MonetaryAmount>;
using ExchangeAmountToCurrencyVector = SmallVector<ExchangeAmountToCurrency, kTypicalNbPrivateAccounts>;
using ExchangeAmountToCurrencyToAmountVector = SmallVector<ExchangeAmountToCurrencyToAmount, kTypicalNbPrivateAccounts>;

template <class VecWithExchangeFirstPos>
ExchangeRetriever::PublicExchangesVec SelectUniquePublicExchanges(ExchangeRetriever exchangeRetriever,
                                                                  VecWithExchangeFirstPos &exchangeVector,
                                                                  bool sort = true) {
  if (sort) {
    // Sort by name is necessary as we want to group private accounts per exchange
    std::ranges::sort(exchangeVector,
                      [](const auto &lhs, const auto &rhs) { return lhs.first->name() < rhs.first->name(); });
  }

  SmallVector<std::string_view, kTypicalNbPrivateAccounts> names(exchangeVector.size());

  std::ranges::transform(exchangeVector, names.begin(),
                         [](const auto &exchangePair) { return exchangePair.first->apiPublic().name(); });

  return exchangeRetriever.selectPublicExchanges(names);
}

}  // namespace

ExchangeHealthCheckStatus ExchangesOrchestrator::healthCheck(ExchangeNameSpan exchangeNames) {
  log::info("Health check for {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeHealthCheckStatus ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [](Exchange *exchange) { return std::make_pair(exchange, exchange->healthCheck()); });

  return ret;
}

ExchangeTickerMaps ExchangesOrchestrator::getTickerInformation(ExchangeNameSpan exchangeNames) {
  log::info("Ticker information for {}", ConstructAccumulatedExchangeNames(exchangeNames));

  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeTickerMaps ret(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
      [](Exchange *exchange) { return std::make_pair(exchange, exchange->queryAllApproximatedOrderBooks(1)); });

  return ret;
}

MarketOrderBookConversionRates ExchangesOrchestrator::getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                                          CurrencyCode equiCurrencyCode,
                                                                          std::optional<int> depth) {
  log::info("Order book of {} on {} requested{}{}", mk, ConstructAccumulatedExchangeNames(exchangeNames),
            equiCurrencyCode.isNeutral() ? "" : " with equi currency ",
            equiCurrencyCode.isNeutral() ? "" : equiCurrencyCode);
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradable;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), isMarketTradable.begin(),
                 [mk](Exchange *exchange) { return exchange->queryTradableMarkets().contains(mk); });

  FilterVector(selectedExchanges, isMarketTradable);

  MarketOrderBookConversionRates ret(selectedExchanges.size());
  auto marketOrderBooksFunc = [mk, equiCurrencyCode, depth](Exchange *exchange) {
    std::optional<MonetaryAmount> optConversionRate =
        equiCurrencyCode.isNeutral() ? std::nullopt
                                     : exchange->apiPublic().convert(MonetaryAmount(1, mk.quote()), equiCurrencyCode);
    MarketOrderBook marketOrderBook(depth ? exchange->queryOrderBook(mk, *depth) : exchange->queryOrderBook(mk));
    if (!optConversionRate && !equiCurrencyCode.isNeutral()) {
      log::warn("Unable to convert {} into {} on {}", marketOrderBook.market().quote(), equiCurrencyCode,
                exchange->name());
    }
    return std::make_tuple(exchange->name(), std::move(marketOrderBook), optConversionRate);
  };
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 marketOrderBooksFunc);
  return ret;
}

BalancePerExchange ExchangesOrchestrator::getBalance(std::span<const ExchangeName> privateExchangeNames,
                                                     const BalanceOptions &balanceOptions) {
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  log::info("Query balance from {}{}{} with{} balance in use", ConstructAccumulatedExchangeNames(privateExchangeNames),
            equiCurrency.isNeutral() ? "" : " with equi currency ", equiCurrency, withBalanceInUse ? "" : "out");

  ExchangeRetriever::SelectedExchanges balanceExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts> balancePortfolios(balanceExchanges.size());
  std::transform(std::execution::par, balanceExchanges.begin(), balanceExchanges.end(), balancePortfolios.begin(),
                 [&](Exchange *exchange) { return exchange->apiPrivate().getAccountBalance(balanceOptions); });

  BalancePerExchange ret;
  ret.reserve(balanceExchanges.size());
  std::transform(balanceExchanges.begin(), balanceExchanges.end(), std::make_move_iterator(balancePortfolios.begin()),
                 std::back_inserter(ret), [](Exchange *exchange, BalancePortfolio &&balancePortfolio) {
                   return std::make_pair(exchange, std::move(balancePortfolio));
                 });

  return ret;
}

WalletPerExchange ExchangesOrchestrator::getDepositInfo(std::span<const ExchangeName> privateExchangeNames,
                                                        CurrencyCode depositCurrency) {
  log::info("Query {} deposit information from {}", depositCurrency,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges depositInfoExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  /// Keep only exchanges which can receive given currency
  SmallVector<bool, kTypicalNbPrivateAccounts> canDepositCurrency(depositInfoExchanges.size());

  auto canDepositFunc = [depositCurrency](Exchange *exchange) {
    auto tradableCur = exchange->queryTradableCurrencies();
    auto curIt = tradableCur.find(depositCurrency);
    if (curIt == tradableCur.end()) {
      return false;
    }
    if (curIt->canDeposit()) {
      log::debug("{} can currently be deposited on {}", curIt->standardCode(), exchange->name());
    } else {
      log::info("{} cannot currently be deposited on {}", curIt->standardCode(), exchange->name());
    }
    return curIt->canDeposit();
  };

  // Do not call in parallel here because tradable currencies service could be queried from several identical public
  // exchanges (when there are several accounts for one exchange)
  std::ranges::transform(depositInfoExchanges, canDepositCurrency.begin(), canDepositFunc);

  FilterVector(depositInfoExchanges, canDepositCurrency);

  SmallVector<Wallet, kTypicalNbPrivateAccounts> walletPerExchange(depositInfoExchanges.size());
  std::transform(
      std::execution::par, depositInfoExchanges.begin(), depositInfoExchanges.end(), walletPerExchange.begin(),
      [depositCurrency](Exchange *exchange) { return exchange->apiPrivate().queryDepositWallet(depositCurrency); });
  WalletPerExchange ret;
  ret.reserve(depositInfoExchanges.size());
  std::transform(depositInfoExchanges.begin(), depositInfoExchanges.end(),
                 std::make_move_iterator(walletPerExchange.begin()), std::back_inserter(ret),
                 [](const Exchange *exchange, Wallet &&wallet) { return std::make_pair(exchange, std::move(wallet)); });
  return ret;
}

OpenedOrdersPerExchange ExchangesOrchestrator::getOpenedOrders(std::span<const ExchangeName> privateExchangeNames,
                                                               const OrdersConstraints &openedOrdersConstraints) {
  log::info("Query opened orders matching {} on {}", openedOrdersConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  OpenedOrdersPerExchange ret(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(), [&](Exchange *exchange) {
        return std::make_pair(exchange, OrdersSet(exchange->apiPrivate().queryOpenedOrders(openedOrdersConstraints)));
      });

  return ret;
}

NbCancelledOrdersPerExchange ExchangesOrchestrator::cancelOrders(std::span<const ExchangeName> privateExchangeNames,
                                                                 const OrdersConstraints &ordersConstraints) {
  log::info("Cancel opened orders matching {} on {}", ordersConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);
  NbCancelledOrdersPerExchange nbOrdersCancelled(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), nbOrdersCancelled.begin(),
                 [&](Exchange *exchange) {
                   return std::make_pair(exchange, exchange->apiPrivate().cancelOpenedOrders(ordersConstraints));
                 });

  return nbOrdersCancelled;
}

DepositsPerExchange ExchangesOrchestrator::getRecentDeposits(std::span<const ExchangeName> privateExchangeNames,
                                                             const DepositsConstraints &depositsConstraints) {
  log::info("Query recent deposits matching {} on {}", depositsConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  DepositsPerExchange ret(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(), [&](Exchange *exchange) {
        return std::make_pair(exchange, DepositsSet(exchange->apiPrivate().queryRecentDeposits(depositsConstraints)));
      });

  return ret;
}

ConversionPathPerExchange ExchangesOrchestrator::getConversionPaths(Market mk, ExchangeNameSpan exchangeNames) {
  log::info("Query {} conversion path from {}", mk, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  ConversionPathPerExchange conversionPathPerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 conversionPathPerExchange.begin(), [mk](Exchange *exchange) {
                   return std::make_pair(exchange, exchange->apiPublic().findMarketsPath(mk.base(), mk.quote()));
                 });

  return conversionPathPerExchange;
}

MarketsPerExchange ExchangesOrchestrator::getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2,
                                                                ExchangeNameSpan exchangeNames) {
  string curStr = cur1.str();
  if (!cur2.isNeutral()) {
    curStr.push_back('-');
    cur2.appendStrTo(curStr);
  }
  log::info("Query markets with {} from {}", curStr, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  MarketsPerExchange marketsPerExchange(selectedExchanges.size());
  auto marketsWithCur = [cur1, cur2](Exchange *exchange) {
    MarketSet markets = exchange->queryTradableMarkets();
    MarketSet ret;
    std::copy_if(markets.begin(), markets.end(), std::inserter(ret, ret.end()),
                 [cur1, cur2](Market mk) { return mk.canTrade(cur1) && (cur2.isNeutral() || mk.canTrade(cur2)); });
    return std::make_pair(exchange, std::move(ret));
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
                 isCurrencyTradablePerExchange.begin(), [currencyCode, shouldBeWithdrawable](Exchange *exchange) {
                   CurrencyExchangeFlatSet currencies = exchange->queryTradableCurrencies();
                   auto foundIt = currencies.find(currencyCode);
                   return foundIt != currencies.end() && (!shouldBeWithdrawable || foundIt->canWithdraw());
                 });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isCurrencyTradablePerExchange);
  return selectedExchanges;
}

UniquePublicSelectedExchanges ExchangesOrchestrator::getExchangesTradingMarket(Market mk,
                                                                               ExchangeNameSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isMarketTradablePerExchange.begin(),
                 [mk](Exchange *exchange) { return exchange->queryTradableMarkets().contains(mk); });

  // Erases Exchanges which do not propose asked market
  FilterVector(selectedExchanges, isMarketTradablePerExchange);

  return selectedExchanges;
}

namespace {
using MarketSetsPerPublicExchange = FixedCapacityVector<MarketSet, kNbSupportedExchanges>;

api::CryptowatchAPI::Fiats QueryFiats(const ExchangeRetriever::PublicExchangesVec &publicExchanges) {
  api::CryptowatchAPI::Fiats fiats;
  if (!publicExchanges.empty()) {
    fiats = publicExchanges.front()->queryFiats();
  }
  return fiats;
}

using MarketSetsPtrPerExchange = SmallVector<MarketSet *, kTypicalNbPrivateAccounts>;

MarketSetsPtrPerExchange MapMarketSetsPtrInExchangesOrder(const ExchangeAmountPairVector &exchangeAmountPairVector,
                                                          const ExchangeRetriever::PublicExchangesVec &publicExchanges,
                                                          MarketSetsPerPublicExchange &marketSetsPerExchange) {
  MarketSetsPtrPerExchange marketSetsPtrFromExchange(exchangeAmountPairVector.size());
  std::transform(exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(), marketSetsPtrFromExchange.begin(),
                 [&](const auto &exchangePair) {
                   auto posIt =
                       std::ranges::find_if(publicExchanges, [&exchangePair](api::ExchangePublic *publicExchange) {
                         return exchangePair.first->name() == publicExchange->name();
                       });
                   return marketSetsPerExchange.data() + (posIt - publicExchanges.begin());
                 });
  return marketSetsPtrFromExchange;
}

using KeepExchangeBoolArray = std::array<bool, kNbSupportedExchanges>;

ExchangeAmountMarketsPathVector FilterConversionPaths(const ExchangeAmountPairVector &exchangeAmountPairVector,
                                                      CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                                      MarketSetsPerPublicExchange &marketsPerPublicExchange,
                                                      const api::CryptowatchAPI::Fiats &fiats,
                                                      const TradeOptions &tradeOptions) {
  ExchangeAmountMarketsPathVector ret;

  int nbExchanges = static_cast<int>(exchangeAmountPairVector.size());
  int publicExchangePos = -1;
  constexpr bool considerStableCoinsAsFiats = false;
  api::ExchangePublic *pExchangePublic = nullptr;
  for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
    const auto &exchangeAmountPair = exchangeAmountPairVector[exchangePos];
    if (pExchangePublic != &exchangeAmountPair.first->apiPublic()) {
      pExchangePublic = &exchangeAmountPair.first->apiPublic();
      ++publicExchangePos;
    }

    MarketSet &markets = marketsPerPublicExchange[publicExchangePos];
    MarketsPath marketsPath =
        pExchangePublic->findMarketsPath(fromCurrency, toCurrency, markets, fiats, considerStableCoinsAsFiats);
    const int nbMarketsInPath = static_cast<int>(marketsPath.size());
    if (nbMarketsInPath == 1 ||
        (nbMarketsInPath > 1 &&
         tradeOptions.isMultiTradeAllowed(pExchangePublic->exchangeInfo().multiTradeAllowedByDefault()))) {
      ret.emplace_back(exchangeAmountPair.first, exchangeAmountPair.second, std::move(marketsPath));
    } else {
      log::warn("{} is not convertible{} to {} on {}", fromCurrency,
                nbMarketsInPath == 0 ? "" : "directly (and multi trade is not allowed)", toCurrency,
                pExchangePublic->name());
    }
  }
  return ret;
}

ExchangeAmountPairVector ComputeExchangeAmountPairVector(CurrencyCode fromCurrency,
                                                         const BalancePerExchange &balancePerExchange) {
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector;

  for (const auto &exchangeBalancePair : balancePerExchange) {
    MonetaryAmount avAmount = exchangeBalancePair.second.get(fromCurrency);
    if (avAmount > 0) {
      exchangeAmountPairVector.emplace_back(exchangeBalancePair.first, avAmount);
    }
  }

  return exchangeAmountPairVector;
}

TradedAmountsPerExchange LaunchAndCollectTrades(ExchangeAmountMarketsPathVector::iterator first,
                                                ExchangeAmountMarketsPathVector::iterator last, CurrencyCode toCurrency,
                                                const TradeOptions &tradeOptions) {
  TradedAmountsPerExchange tradeAmountsPerExchange(std::distance(first, last));
  std::transform(
      std::execution::par, first, last, tradeAmountsPerExchange.begin(), [toCurrency, &tradeOptions](auto &tuple) {
        Exchange *exchange = std::get<0>(tuple);
        return std::make_pair(
            exchange, exchange->apiPrivate().trade(std::get<1>(tuple), toCurrency, tradeOptions, std::get<2>(tuple)));
      });
  return tradeAmountsPerExchange;
}

template <class Iterator>
TradedAmountsPerExchange LaunchAndCollectTrades(Iterator first, Iterator last, const TradeOptions &tradeOptions) {
  TradedAmountsPerExchange tradeAmountsPerExchange(std::distance(first, last));
  std::transform(std::execution::par, first, last, tradeAmountsPerExchange.begin(), [&tradeOptions](auto &tuple) {
    Exchange *exchange = std::get<0>(tuple);
    return std::make_pair(exchange, exchange->apiPrivate().trade(std::get<1>(tuple), std::get<2>(tuple), tradeOptions,
                                                                 std::get<3>(tuple)));
  });
  return tradeAmountsPerExchange;
}

ExchangeAmountMarketsPathVector CreateExchangeAmountMarketsPathVector(ExchangeRetriever exchangeRetriever,
                                                                      const BalancePerExchange &balancePerExchange,
                                                                      CurrencyCode fromCurrency,
                                                                      CurrencyCode toCurrency,
                                                                      const TradeOptions &tradeOptions) {
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector = ComputeExchangeAmountPairVector(fromCurrency, balancePerExchange);

  ExchangeRetriever::PublicExchangesVec publicExchanges =
      SelectUniquePublicExchanges(exchangeRetriever, exchangeAmountPairVector);

  MarketSetsPerPublicExchange marketsPerPublicExchange(publicExchanges.size());

  api::CryptowatchAPI::Fiats fiats = QueryFiats(publicExchanges);

  return FilterConversionPaths(exchangeAmountPairVector, fromCurrency, toCurrency, marketsPerPublicExchange, fiats,
                               tradeOptions);
}

}  // namespace

TradedAmountsPerExchange ExchangesOrchestrator::trade(MonetaryAmount startAmount, bool isPercentageTrade,
                                                      CurrencyCode toCurrency,
                                                      std::span<const ExchangeName> privateExchangeNames,
                                                      const TradeOptions &tradeOptions) {
  if (privateExchangeNames.size() == 1 && !isPercentageTrade) {
    // In this special case we don't need to call the balance - call trade directly
    Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeNames.front());
    return {1, std::make_pair(std::addressof(exchange),
                              exchange.apiPrivate().trade(startAmount, toCurrency, tradeOptions))};
  }

  const CurrencyCode fromCurrency = startAmount.currencyCode();

  ExchangeAmountMarketsPathVector exchangeAmountMarketsPathVector = CreateExchangeAmountMarketsPathVector(
      _exchangeRetriever, getBalance(privateExchangeNames), fromCurrency, toCurrency, tradeOptions);

  MonetaryAmount currentTotalAmount(0, fromCurrency);

  auto it = exchangeAmountMarketsPathVector.begin();
  if (!exchangeAmountMarketsPathVector.empty()) {
    // Sort exchanges from largest to lowest available amount (should be after filter on markets and conversion paths)
    std::ranges::stable_sort(exchangeAmountMarketsPathVector,
                             [](const auto &lhs, const auto &rhs) { return std::get<1>(lhs) > std::get<1>(rhs); });

    // Locate the point where there is enough available amount to trade for this currency
    if (isPercentageTrade) {
      MonetaryAmount totalAvailableAmount = std::accumulate(
          exchangeAmountMarketsPathVector.begin(), exchangeAmountMarketsPathVector.end(), currentTotalAmount,
          [](MonetaryAmount tot, const auto &tuple) { return tot + std::get<1>(tuple); });
      startAmount = (totalAvailableAmount * startAmount.toNeutral()) / 100;
    }
    for (auto endIt = exchangeAmountMarketsPathVector.end(); it != endIt && currentTotalAmount < startAmount; ++it) {
      MonetaryAmount &amount = std::get<1>(*it);
      if (currentTotalAmount + amount > startAmount) {
        // Cap last amount such that total start trade on all exchanges reaches exactly 'startAmount'
        amount = startAmount - currentTotalAmount;
      }
      currentTotalAmount += amount;
    }
  }

  if (currentTotalAmount == 0) {
    log::warn("No available {} to trade", fromCurrency);
  } else if (currentTotalAmount < startAmount) {
    log::warn("Will trade {} < {} amount", currentTotalAmount, startAmount);
  }

  /// We have enough total available amount. Launch all trades in parallel
  return LaunchAndCollectTrades(exchangeAmountMarketsPathVector.begin(), it, toCurrency, tradeOptions);
}

TradedAmountsPerExchange ExchangesOrchestrator::smartBuy(MonetaryAmount endAmount,
                                                         std::span<const ExchangeName> privateExchangeNames,
                                                         const TradeOptions &tradeOptions) {
  const CurrencyCode toCurrency = endAmount.currencyCode();
  BalancePerExchange balancePerExchange = getBalance(privateExchangeNames);

  // Keep only exchanges which have some amount on at least one of the preferred payment currencies
  SmallVector<bool, kTypicalNbPrivateAccounts> exchangesWithSomePreferredPaymentCurrency(balancePerExchange.size());
  std::ranges::transform(
      balancePerExchange, exchangesWithSomePreferredPaymentCurrency.begin(), [](auto &exchangeBalancePair) {
        return std::ranges::any_of(exchangeBalancePair.first->exchangeInfo().preferredPaymentCurrencies(),
                                   [&](CurrencyCode cur) { return exchangeBalancePair.second.hasSome(cur); });
      });
  FilterVector(balancePerExchange, exchangesWithSomePreferredPaymentCurrency);

  ExchangeRetriever::PublicExchangesVec publicExchanges =
      SelectUniquePublicExchanges(_exchangeRetriever, balancePerExchange);

  MarketSetsPerPublicExchange marketsPerPublicExchange(publicExchanges.size());

  FixedCapacityVector<MarketOrderBookMap, kNbSupportedExchanges> marketOrderbooksPerPublicExchange(
      publicExchanges.size());

  api::CryptowatchAPI::Fiats fiats = QueryFiats(publicExchanges);

  ExchangeAmountToCurrencyToAmountVector trades;
  MonetaryAmount remEndAmount = endAmount;
  constexpr bool canUseCryptowatchAPI = false;
  constexpr bool considerStableCoinsAsFiats = false;
  for (int nbSteps = 1;; ++nbSteps) {
    bool continuingHigherStepsPossible = false;
    const int nbTrades = trades.size();
    int publicExchangePos = -1;
    api::ExchangePublic *pExchangePublic = nullptr;
#ifdef CCT_CLANG
    // Clang does not consider structured bindings symbols as variables yet...
    // So they cannot be captured by lambdas
    for (auto &pExchangeBalancePair : balancePerExchange) {
      auto &pExchange = pExchangeBalancePair.first;
      auto &balance = pExchangeBalancePair.second;
#else
    for (auto &[pExchange, balance] : balancePerExchange) {
#endif
      if (pExchangePublic != &pExchange->apiPublic()) {
        pExchangePublic = &pExchange->apiPublic();
        ++publicExchangePos;
      }
      api::ExchangePublic &exchangePublic = *pExchangePublic;
      if (nbSteps > 1 &&
          !tradeOptions.isMultiTradeAllowed(exchangePublic.exchangeInfo().multiTradeAllowedByDefault())) {
        continue;
      }
      auto &markets = marketsPerPublicExchange[publicExchangePos];
      auto &marketOrderBookMap = marketOrderbooksPerPublicExchange[publicExchangePos];
      for (CurrencyCode fromCurrency : pExchange->exchangeInfo().preferredPaymentCurrencies()) {
        if (fromCurrency == toCurrency) {
          continue;
        }
        MonetaryAmount avAmount = balance.get(fromCurrency);
        if (avAmount > 0 &&
            std::none_of(trades.begin(), trades.begin() + nbTrades, [pExchange, fromCurrency](const auto &tuple) {
              return std::get<0>(tuple) == pExchange && std::get<1>(tuple).currencyCode() == fromCurrency;
            })) {
          auto conversionPath =
              exchangePublic.findMarketsPath(fromCurrency, toCurrency, markets, fiats, considerStableCoinsAsFiats);
          const int nbConversions = static_cast<int>(conversionPath.size());
          if (nbConversions > nbSteps) {
            continuingHigherStepsPossible = true;
          } else if (nbConversions == nbSteps) {
            MonetaryAmount startAmount = avAmount;
            std::optional<MonetaryAmount> optEndAmount =
                exchangePublic.convert(startAmount, toCurrency, conversionPath, fiats, marketOrderBookMap,
                                       canUseCryptowatchAPI, tradeOptions.priceOptions());
            if (optEndAmount) {
              trades.emplace_back(pExchange, startAmount, toCurrency, std::move(conversionPath), *optEndAmount);
            }
          }
        }
      }
    }
    // Sort exchanges from largest to lowest end amount
    std::stable_sort(trades.begin() + nbTrades, trades.end(),
                     [](const auto &lhs, const auto &rhs) { return std::get<4>(lhs) > std::get<4>(rhs); });
    int nbTradesToKeep = 0;
    for (auto &[pExchange, startAmount, tradeToCurrency, conversionPath, tradeEndAmount] : trades) {
      if (tradeEndAmount > remEndAmount) {
        startAmount = (startAmount * remEndAmount.toNeutral()) / tradeEndAmount.toNeutral();
        tradeEndAmount = remEndAmount;
      }
      remEndAmount -= tradeEndAmount;

      log::debug("Validating max trade of {} to {} on {}_{}", startAmount, tradeEndAmount, pExchange->name(),
                 pExchange->keyName());

      ++nbTradesToKeep;
      if (remEndAmount == 0) {
        break;
      }
    }
    trades.erase(trades.begin() + nbTradesToKeep, trades.end());

    if (remEndAmount == 0 || !continuingHigherStepsPossible) {
      break;
    }
  }

  if (remEndAmount != 0) {
    log::warn("Will trade {} < {} amount", endAmount - remEndAmount, endAmount);
  }

  return LaunchAndCollectTrades(trades.begin(), trades.end(), tradeOptions);
}

TradedAmountsPerExchange ExchangesOrchestrator::smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                                          std::span<const ExchangeName> privateExchangeNames,
                                                          const TradeOptions &tradeOptions) {
  const CurrencyCode fromCurrency = startAmount.currencyCode();
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector =
      ComputeExchangeAmountPairVector(fromCurrency, getBalance(privateExchangeNames));

  ExchangeAmountToCurrencyVector trades;
  MonetaryAmount remStartAmount = startAmount;
  if (!exchangeAmountPairVector.empty()) {
    // Sort exchanges from largest to lowest available amount
    std::ranges::stable_sort(exchangeAmountPairVector,
                             [](const auto &lhs, const auto &rhs) { return lhs.second > rhs.second; });

    ExchangeRetriever::PublicExchangesVec publicExchanges =
        SelectUniquePublicExchanges(_exchangeRetriever, exchangeAmountPairVector, false);  // unsorted

    MarketSetsPerPublicExchange marketsPerPublicExchange(publicExchanges.size());

    // As we want to sort Exchanges by largest to smallest amount, we cannot directly map MarketSets per Exchange.
    // That's why we need to keep pointers to MarketSets ordered by exchanges
    MarketSetsPtrPerExchange marketSetsPtrPerExchange =
        MapMarketSetsPtrInExchangesOrder(exchangeAmountPairVector, publicExchanges, marketsPerPublicExchange);

    api::CryptowatchAPI::Fiats fiats = QueryFiats(publicExchanges);

    if (isPercentageTrade) {
      MonetaryAmount totalAvailableAmount = std::accumulate(
          exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(), MonetaryAmount(0, fromCurrency),
          [](MonetaryAmount tot, const auto &tuple) { return tot + std::get<1>(tuple); });
      startAmount = (totalAvailableAmount * startAmount.toNeutral()) / 100;
      remStartAmount = startAmount;
    }

    // check from which exchanges we can start trades, minimizing number of steps per trade
    constexpr bool considerStableCoinsAsFiats = false;
    for (int nbSteps = 1;; ++nbSteps) {
      bool continuingHigherStepsPossible = false;
      int exchangePos = 0;
      for (auto &[pExchange, avAmount] : exchangeAmountPairVector) {
        if (avAmount == 0 ||  // It can be set to 0 in below code
            (nbSteps > 1 &&
             !tradeOptions.isMultiTradeAllowed(pExchange->exchangeInfo().multiTradeAllowedByDefault()))) {
          ++exchangePos;
          continue;
        }
        MarketSet &markets = *marketSetsPtrPerExchange[exchangePos];
        for (CurrencyCode toCurrency : pExchange->exchangeInfo().preferredPaymentCurrencies()) {
          if (fromCurrency == toCurrency) {
            continue;
          }
          MarketsPath path = pExchange->apiPublic().findMarketsPath(fromCurrency, toCurrency, markets, fiats,
                                                                    considerStableCoinsAsFiats);
          if (static_cast<int>(path.size()) > nbSteps) {
            continuingHigherStepsPossible = true;
          } else if (static_cast<int>(path.size()) == nbSteps) {
            MonetaryAmount fromAmount = avAmount;
            if (fromAmount > remStartAmount) {
              fromAmount = remStartAmount;
            }
            remStartAmount -= fromAmount;
            trades.emplace_back(pExchange, fromAmount, toCurrency, std::move(path));
            avAmount = MonetaryAmount(0, fromCurrency);
            if (remStartAmount == 0) {
              break;
            }
          }
        }
        if (remStartAmount == 0) {
          break;
        }
        ++exchangePos;
      }
      if (remStartAmount == 0 || !continuingHigherStepsPossible) {
        break;
      }
    }
  }

  if (remStartAmount == startAmount) {
    log::warn("No available amount of {} to sell", startAmount.currencyCode());
  } else if (remStartAmount != 0) {
    log::warn("Will trade {} < {} amount", startAmount - remStartAmount, startAmount);
  }

  return LaunchAndCollectTrades(trades.begin(), trades.end(), tradeOptions);
}

TradedAmountsVectorWithFinalAmountPerExchange ExchangesOrchestrator::dustSweeper(
    std::span<const ExchangeName> privateExchangeNames, CurrencyCode currencyCode) {
  log::info("Query {} dust sweeper from {}", currencyCode, ConstructAccumulatedExchangeNames(privateExchangeNames));

  ExchangeRetriever::SelectedExchanges selExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  TradedAmountsVectorWithFinalAmountPerExchange ret(selExchanges.size());
  std::transform(std::execution::par, selExchanges.begin(), selExchanges.end(), ret.begin(),
                 [currencyCode](Exchange *exchange) {
                   return std::make_pair(static_cast<const Exchange *>(exchange),
                                         exchange->apiPrivate().queryDustSweeper(currencyCode));
                 });

  return ret;
}

WithdrawInfo ExchangesOrchestrator::withdraw(MonetaryAmount grossAmount, bool isPercentageWithdraw,
                                             const ExchangeName &fromPrivateExchangeName,
                                             const ExchangeName &toPrivateExchangeName,
                                             const WithdrawOptions &withdrawOptions) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  if (isPercentageWithdraw) {
    log::info("Withdraw gross {}% {} from {} to {} requested", grossAmount.amountStr(), currencyCode,
              fromPrivateExchangeName, toPrivateExchangeName);
  } else {
    log::info("Withdraw gross {} from {} to {} requested", grossAmount, fromPrivateExchangeName, toPrivateExchangeName);
  }

  Exchange &fromExchange = _exchangeRetriever.retrieveUniqueCandidate(fromPrivateExchangeName);
  Exchange &toExchange = _exchangeRetriever.retrieveUniqueCandidate(toPrivateExchangeName);
  const std::array<Exchange *, 2> exchangePair = {std::addressof(fromExchange), std::addressof(toExchange)};
  if (exchangePair.front() == exchangePair.back()) {
    throw exception("Cannot withdraw to the same account");
  }
  std::array<CurrencyExchangeFlatSet, 2> currencyExchangeSets;
  std::transform(std::execution::par, exchangePair.begin(), exchangePair.end(), currencyExchangeSets.begin(),
                 [](Exchange *exchange) { return exchange->queryTradableCurrencies(); });

  if (!fromExchange.canWithdraw(currencyCode, currencyExchangeSets.front())) {
    string errMsg("It's currently not possible to withdraw ");
    currencyCode.appendStrTo(errMsg);
    errMsg.append(" from ").append(fromPrivateExchangeName.str());
    log::error(errMsg);
    return WithdrawInfo(std::move(errMsg));
  }
  if (!toExchange.canDeposit(currencyCode, currencyExchangeSets.back())) {
    string errMsg("It's currently not possible to deposit ");
    currencyCode.appendStrTo(errMsg);
    errMsg.append(" to ").append(fromPrivateExchangeName.str());
    log::error(errMsg);
    return WithdrawInfo(std::move(errMsg));
  }

  if (isPercentageWithdraw) {
    MonetaryAmount avAmount = fromExchange.apiPrivate().getAccountBalance().get(currencyCode);
    grossAmount = (avAmount * grossAmount.toNeutral()) / 100;
  }

  return fromExchange.apiPrivate().withdraw(grossAmount, toExchange.apiPrivate(), withdrawOptions);
}

MonetaryAmountPerExchange ExchangesOrchestrator::getWithdrawFees(CurrencyCode currencyCode,
                                                                 ExchangeNameSpan exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames, true);

  MonetaryAmountPerExchange withdrawFeePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 withdrawFeePerExchange.begin(), [currencyCode](Exchange *exchange) {
                   return std::make_pair(exchange, exchange->queryWithdrawalFee(currencyCode));
                 });
  return withdrawFeePerExchange;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLast24hTradedVolumePerExchange(Market mk,
                                                                                   ExchangeNameSpan exchangeNames) {
  log::info("Query last 24h traded volume of {} pair on {}", mk, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 tradedVolumePerExchange.begin(),
                 [mk](Exchange *exchange) { return std::make_pair(exchange, exchange->queryLast24hVolume(mk)); });
  return tradedVolumePerExchange;
}

LastTradesPerExchange ExchangesOrchestrator::getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames,
                                                                      int nbLastTrades) {
  log::info("Query {} last trades on {} volume from {}", nbLastTrades, mk,
            ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  LastTradesPerExchange ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [mk, nbLastTrades](Exchange *exchange) {
                   return std::make_pair(static_cast<const Exchange *>(exchange),
                                         exchange->queryLastTrades(mk, nbLastTrades));
                 });

  return ret;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  log::info("Query last price from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), lastPricePerExchange.begin(),
                 [mk](Exchange *exchange) { return std::make_pair(exchange, exchange->queryLastPrice(mk)); });
  return lastPricePerExchange;
}

}  // namespace cct
