#include "exchangesorchestrator.hpp"

#include <algorithm>
#include <array>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <span>
#include <string_view>
#include <tuple>
#include <utility>

#include "balanceoptions.hpp"
#include "balanceportfolio.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_log.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "currencycode.hpp"
#include "currencycodeset.hpp"
#include "currencyexchangeflatset.hpp"
#include "depositsconstraints.hpp"
#include "exchange-name-enum.hpp"
#include "exchange-names.hpp"
#include "exchange.hpp"
#include "exchangename.hpp"
#include "exchangeprivateapitypes.hpp"
#include "exchangepublicapi.hpp"
#include "exchangepublicapitypes.hpp"
#include "exchangeretriever.hpp"
#include "market-timestamp-set.hpp"
#include "market-trader-engine.hpp"
#include "market-trading-global-result.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "monetaryamountbycurrencyset.hpp"
#include "ordersconstraints.hpp"
#include "queryresulttypes.hpp"
#include "replay-options.hpp"
#include "requests-config.hpp"
#include "threadpool.hpp"
#include "time-window.hpp"
#include "trade-range-stats.hpp"
#include "tradedamounts.hpp"
#include "tradeoptions.hpp"
#include "traderesult.hpp"
#include "wallet.hpp"
#include "withdrawinfo.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"

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

struct ExchangeAmountMarkets {
  Exchange *exchange;
  MonetaryAmount amount;
  MarketsPath marketsPath;

  using trivially_relocatable = is_trivially_relocatable<MarketsPath>::type;
};

using ExchangeAmountMarketsPathVector = SmallVector<ExchangeAmountMarkets, kTypicalNbPrivateAccounts>;

struct ExchangeAmountToCurrency {
  Exchange *exchange;
  MonetaryAmount amount;
  CurrencyCode currency;
  MarketsPath marketsPath;

  using trivially_relocatable = is_trivially_relocatable<MarketsPath>::type;
};

using ExchangeAmountToCurrencyVector = SmallVector<ExchangeAmountToCurrency, kTypicalNbPrivateAccounts>;

struct ExchangeAmountToCurrencyToAmount {
  Exchange *exchange;
  MonetaryAmount amount;
  CurrencyCode currency;
  MarketsPath marketsPath;
  MonetaryAmount endAmount;

  using trivially_relocatable = is_trivially_relocatable<MarketsPath>::type;
};

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

ExchangesOrchestrator::ExchangesOrchestrator(const schema::RequestsConfig &requestsConfig,
                                             std::span<Exchange> exchangesSpan)
    : _exchangeRetriever(exchangesSpan),
      _threadPool(std::min(requestsConfig.concurrency.nbMaxParallelRequests, static_cast<int>(exchangesSpan.size()))) {
  log::debug("Created a thread pool with {} workers for exchange requests", _threadPool.nbWorkers());
}

ExchangeHealthCheckStatus ExchangesOrchestrator::healthCheck(ExchangeNameSpan exchangeNames) {
  log::info("Health check for {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeHealthCheckStatus ret(selectedExchanges.size());

  _threadPool.parallelTransform(selectedExchanges, ret.begin(),
                                [](Exchange *exchange) { return std::make_pair(exchange, exchange->healthCheck()); });

  return ret;
}

ExchangeTickerMaps ExchangesOrchestrator::getTickerInformation(ExchangeNameSpan exchangeNames) {
  log::info("Ticker information for {}", ConstructAccumulatedExchangeNames(exchangeNames));

  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeTickerMaps ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [](Exchange *exchange) {
    return std::make_pair(exchange, exchange->queryAllApproximatedOrderBooks(1));
  });

  return ret;
}

MarketOrderBookConversionRates ExchangesOrchestrator::getMarketOrderBooks(Market mk, ExchangeNameSpan exchangeNames,
                                                                          CurrencyCode equiCurrencyCode,
                                                                          std::optional<int> depth) {
  const auto actualDepth = depth.value_or(api::ExchangePublic::kDefaultDepth);
  log::info("{} order book of depth {} on {} requested{}{}", mk, actualDepth,
            ConstructAccumulatedExchangeNames(exchangeNames),
            equiCurrencyCode.isNeutral() ? "" : " with equi currency ",
            equiCurrencyCode.isNeutral() ? "" : equiCurrencyCode);
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradable;
  _threadPool.parallelTransform(selectedExchanges, isMarketTradable.begin(),
                                [mk](Exchange *exchange) { return exchange->queryTradableMarkets().contains(mk); });

  FilterVector(selectedExchanges, isMarketTradable);

  MarketOrderBookConversionRates ret(selectedExchanges.size());
  auto marketOrderBooksFunc = [mk, equiCurrencyCode, actualDepth](Exchange *exchange) {
    std::optional<MonetaryAmount> optConversionRate =
        equiCurrencyCode.isNeutral()
            ? std::nullopt
            : exchange->apiPublic().estimatedConvert(MonetaryAmount(1, mk.quote()), equiCurrencyCode);
    if (!optConversionRate && !equiCurrencyCode.isNeutral()) {
      log::warn("Unable to convert {} into {} on {}", mk.quote(), equiCurrencyCode, exchange->name());
    }
    return std::make_tuple(exchange->exchangeNameEnum(), exchange->getOrderBook(mk, actualDepth), optConversionRate);
  };
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), marketOrderBooksFunc);
  return ret;
}

BalancePerExchange ExchangesOrchestrator::getBalance(ExchangeNameSpan privateExchangeNames,
                                                     const BalanceOptions &balanceOptions) {
  CurrencyCode equiCurrency = balanceOptions.equiCurrency();
  bool withBalanceInUse =
      balanceOptions.amountIncludePolicy() == BalanceOptions::AmountIncludePolicy::kWithBalanceInUse;
  log::info("Query balance from {}{}{} with{} balance in use", ConstructAccumulatedExchangeNames(privateExchangeNames),
            equiCurrency.isNeutral() ? "" : " with equi currency ", equiCurrency, withBalanceInUse ? "" : "out");

  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts> balancePortfolios(selectedExchanges.size());

  _threadPool.parallelTransform(selectedExchanges, balancePortfolios.begin(), [&balanceOptions](Exchange *exchange) {
    return exchange->apiPrivate().getAccountBalance(balanceOptions);
  });

  BalancePerExchange ret;
  ret.reserve(selectedExchanges.size());
  // Note: we can use std::ranges::transform with balancePortfolios | std::views::as_rvalues in C++23
  std::transform(selectedExchanges.begin(), selectedExchanges.end(), std::make_move_iterator(balancePortfolios.begin()),
                 std::back_inserter(ret), [](Exchange *exchange, BalancePortfolio &&balancePortfolio) {
                   return std::make_pair(exchange, std::move(balancePortfolio));
                 });

  return ret;
}

WalletPerExchange ExchangesOrchestrator::getDepositInfo(ExchangeNameSpan privateExchangeNames,
                                                        CurrencyCode depositCurrency) {
  log::info("Query {} deposit information from {}", depositCurrency,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges depositInfoExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

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
  _threadPool.parallelTransform(depositInfoExchanges, walletPerExchange.begin(), [depositCurrency](Exchange *exchange) {
    return exchange->apiPrivate().queryDepositWallet(depositCurrency);
  });
  WalletPerExchange ret;
  ret.reserve(depositInfoExchanges.size());
  // Note: we can use std::ranges::transform with walletPerExchange | std::views::as_rvalues in C++23
  std::transform(depositInfoExchanges.begin(), depositInfoExchanges.end(),
                 std::make_move_iterator(walletPerExchange.begin()), std::back_inserter(ret),
                 [](const Exchange *exchange, Wallet &&wallet) { return std::make_pair(exchange, std::move(wallet)); });
  return ret;
}

ClosedOrdersPerExchange ExchangesOrchestrator::getClosedOrders(ExchangeNameSpan privateExchangeNames,
                                                               const OrdersConstraints &closedOrdersConstraints) {
  log::info("Query closed orders matching {} on {}", closedOrdersConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  ClosedOrdersPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [&](Exchange *exchange) {
    return std::make_pair(exchange, ClosedOrderSet(exchange->apiPrivate().queryClosedOrders(closedOrdersConstraints)));
  });

  return ret;
}

OpenedOrdersPerExchange ExchangesOrchestrator::getOpenedOrders(ExchangeNameSpan privateExchangeNames,
                                                               const OrdersConstraints &openedOrdersConstraints) {
  log::info("Query opened orders matching {} on {}", openedOrdersConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  OpenedOrdersPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [&](Exchange *exchange) {
    return std::make_pair(exchange, OpenedOrderSet(exchange->apiPrivate().queryOpenedOrders(openedOrdersConstraints)));
  });

  return ret;
}

NbCancelledOrdersPerExchange ExchangesOrchestrator::cancelOrders(ExchangeNameSpan privateExchangeNames,
                                                                 const OrdersConstraints &ordersConstraints) {
  log::info("Cancel opened orders matching {} on {}", ordersConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);
  NbCancelledOrdersPerExchange nbOrdersCancelled(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, nbOrdersCancelled.begin(), [&](Exchange *exchange) {
    return std::make_pair(exchange, exchange->apiPrivate().cancelOpenedOrders(ordersConstraints));
  });

  return nbOrdersCancelled;
}

DepositsPerExchange ExchangesOrchestrator::getRecentDeposits(ExchangeNameSpan privateExchangeNames,
                                                             const DepositsConstraints &depositsConstraints) {
  log::info("Query recent deposits matching {} on {}", depositsConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  DepositsPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [&](Exchange *exchange) {
    return std::make_pair(exchange, exchange->apiPrivate().queryRecentDeposits(depositsConstraints));
  });

  return ret;
}

WithdrawsPerExchange ExchangesOrchestrator::getRecentWithdraws(ExchangeNameSpan privateExchangeNames,
                                                               const WithdrawsConstraints &withdrawsConstraints) {
  log::info("Query recent withdraws matching {} on {}", withdrawsConstraints,
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges selectedExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  WithdrawsPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [&](Exchange *exchange) {
    return std::make_pair(exchange, exchange->apiPrivate().queryRecentWithdraws(withdrawsConstraints));
  });

  return ret;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getConversion(MonetaryAmount amount, CurrencyCode targetCurrencyCode,
                                                               ExchangeNameEnumSpan exchangeNameEnums) {
  log::info("Query {} conversion into {} from {}", amount, targetCurrencyCode,
            ConstructAccumulatedExchangeNames(exchangeNameEnums));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNameEnums);
  MonetaryAmountPerExchange convertedAmountPerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(
      selectedExchanges, convertedAmountPerExchange.begin(), [amount, targetCurrencyCode](Exchange *exchange) {
        const auto optConvertedAmount = exchange->apiPublic().estimatedConvert(amount, targetCurrencyCode);
        return std::make_pair(exchange, optConvertedAmount.value_or(MonetaryAmount{}));
      });

  return convertedAmountPerExchange;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getConversion(
    std::span<const MonetaryAmount> monetaryAmountPerExchangeToConvert, CurrencyCode targetCurrencyCode,
    ExchangeNameEnumSpan exchangeNameEnums) {
  log::info("Query multiple conversions into {} from {}", targetCurrencyCode,
            ConstructAccumulatedExchangeNames(exchangeNameEnums));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNameEnums);
  MonetaryAmountPerExchange convertedAmountPerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(
      selectedExchanges, convertedAmountPerExchange.begin(),
      [monetaryAmountPerExchangeToConvert, targetCurrencyCode](Exchange *exchange) {
        const auto startAmount = monetaryAmountPerExchangeToConvert[exchange->publicExchangePos()];
        const auto optConvertedAmount = startAmount.isDefault()
                                            ? std::nullopt
                                            : exchange->apiPublic().estimatedConvert(startAmount, targetCurrencyCode);
        return std::make_pair(exchange, optConvertedAmount.value_or(MonetaryAmount{}));
      });

  return convertedAmountPerExchange;
}

ConversionPathPerExchange ExchangesOrchestrator::getConversionPaths(Market mk, ExchangeNameSpan exchangeNames) {
  log::info("Query {} conversion path from {}", mk, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  ConversionPathPerExchange conversionPathPerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, conversionPathPerExchange.begin(), [mk](Exchange *exchange) {
    return std::make_pair(exchange, exchange->apiPublic().findMarketsPath(mk.base(), mk.quote()));
  });

  return conversionPathPerExchange;
}

CurrenciesPerExchange ExchangesOrchestrator::getCurrenciesPerExchange(ExchangeNameSpan exchangeNames) {
  log::info("Get all tradable currencies for {}", ConstructAccumulatedExchangeNames(exchangeNames));

  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  CurrenciesPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [](Exchange *exchange) {
    return std::make_pair(exchange, exchange->queryTradableCurrencies());
  });

  return ret;
}

MarketsPerExchange ExchangesOrchestrator::getMarketsPerExchange(CurrencyCode cur1, CurrencyCode cur2,
                                                                ExchangeNameSpan exchangeNames) {
  string curStr;
  if (!cur1.isNeutral()) {
    curStr.append(" matching ");
    cur1.appendStrTo(curStr);
    if (!cur2.isNeutral()) {
      curStr.push_back('-');
      cur2.appendStrTo(curStr);
    }
  }

  log::info("Query markets{} from {}", curStr, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  MarketsPerExchange marketsPerExchange(selectedExchanges.size());
  auto marketsWithCur = [cur1, cur2](Exchange *exchange) {
    MarketSet markets = exchange->queryTradableMarkets();
    MarketSet ret;
    std::ranges::copy_if(markets, std::inserter(ret, ret.end()), [cur1, cur2](Market mk) {
      return (cur1.isNeutral() || mk.canTrade(cur1)) && (cur2.isNeutral() || mk.canTrade(cur2));
    });
    return std::make_pair(exchange, std::move(ret));
  };
  _threadPool.parallelTransform(selectedExchanges, marketsPerExchange.begin(), marketsWithCur);
  return marketsPerExchange;
}

UniquePublicSelectedExchanges ExchangesOrchestrator::getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                                                 ExchangeNameSpan exchangeNames,
                                                                                 bool shouldBeWithdrawable) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isCurrencyTradablePerExchange;
  _threadPool.parallelTransform(selectedExchanges, isCurrencyTradablePerExchange.begin(),
                                [currencyCode, shouldBeWithdrawable](Exchange *exchange) {
                                  if (currencyCode.isNeutral()) {
                                    return true;
                                  }
                                  CurrencyExchangeFlatSet currencies = exchange->queryTradableCurrencies();
                                  auto foundIt = currencies.find(currencyCode);
                                  return foundIt != currencies.end() &&
                                         (!shouldBeWithdrawable || foundIt->canWithdraw());
                                });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isCurrencyTradablePerExchange);
  return selectedExchanges;
}

UniquePublicSelectedExchanges ExchangesOrchestrator::getExchangesTradingMarket(Market mk,
                                                                               ExchangeNameSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradablePerExchange;
  _threadPool.parallelTransform(selectedExchanges, isMarketTradablePerExchange.begin(),
                                [mk](Exchange *exchange) { return exchange->queryTradableMarkets().contains(mk); });

  // Erases Exchanges which do not propose asked market
  FilterVector(selectedExchanges, isMarketTradablePerExchange);

  return selectedExchanges;
}

namespace {
using MarketSetsPerPublicExchange = FixedCapacityVector<MarketSet, kNbSupportedExchanges>;

auto QueryFiats(const ExchangeRetriever::PublicExchangesVec &publicExchanges) {
  CurrencyCodeSet fiats;
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
  std::ranges::transform(exchangeAmountPairVector, marketSetsPtrFromExchange.begin(), [&](const auto &exchangePair) {
    auto posIt = std::ranges::find_if(publicExchanges, [&exchangePair](api::ExchangePublic *publicExchange) {
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
                                                      const CurrencyCodeSet &fiats, const TradeOptions &tradeOptions) {
  ExchangeAmountMarketsPathVector ret;

  int publicExchangePos = -1;
  api::ExchangePublic *pExchangePublic = nullptr;
  for (const auto &[exchangePtr, exchangeAmount] : exchangeAmountPairVector) {
    if (pExchangePublic != &exchangePtr->apiPublic()) {
      pExchangePublic = &exchangePtr->apiPublic();
      ++publicExchangePos;
    }
    api::ExchangePublic &exchangePublic = *pExchangePublic;

    MarketSet &markets = marketsPerPublicExchange[publicExchangePos];
    MarketsPath marketsPath = exchangePublic.findMarketsPath(fromCurrency, toCurrency, markets, fiats,
                                                             api::ExchangePublic::MarketPathMode::kStrict);
    const int nbMarketsInPath = static_cast<int>(marketsPath.size());
    if (nbMarketsInPath == 1 ||
        (nbMarketsInPath > 1 &&
         tradeOptions.isMultiTradeAllowed(exchangePublic.exchangeConfig().query.multiTradeAllowedByDefault))) {
      ret.emplace_back(exchangePtr, exchangeAmount, std::move(marketsPath));
    } else {
      log::warn("{} is not convertible{} to {} on {}", fromCurrency,
                nbMarketsInPath == 0 ? "" : "directly (and multi trade is not allowed)", toCurrency,
                exchangePublic.name());
    }
  }
  return ret;
}

ExchangeAmountPairVector ComputeExchangeAmountPairVector(CurrencyCode fromCurrency,
                                                         const BalancePerExchange &balancePerExchange) {
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector;

  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    MonetaryAmount avAmount = balancePortfolio.get(fromCurrency);
    if (avAmount > 0) {
      exchangeAmountPairVector.emplace_back(exchangePtr, avAmount);
    }
  }

  return exchangeAmountPairVector;
}

TradeResultPerExchange LaunchAndCollectTrades(ThreadPool &threadPool, std::span<ExchangeAmountMarkets> input,
                                              CurrencyCode toCurrency, const TradeOptions &tradeOptions) {
  TradeResultPerExchange tradeResultPerExchange(static_cast<TradeResultPerExchange::size_type>(input.size()));

  threadPool.parallelTransform(input, tradeResultPerExchange.begin(),
                               [toCurrency, &tradeOptions](ExchangeAmountMarkets &exchangeAmountMarketsPath) {
                                 Exchange *exchange = exchangeAmountMarketsPath.exchange;
                                 const MonetaryAmount from = exchangeAmountMarketsPath.amount;
                                 const auto &marketsPath = exchangeAmountMarketsPath.marketsPath;

                                 TradedAmounts tradedAmounts =
                                     exchange->apiPrivate().trade(from, toCurrency, tradeOptions, marketsPath);

                                 return std::make_pair(exchange, TradeResult(std::move(tradedAmounts), from));
                               });
  return tradeResultPerExchange;
}

TradeResultPerExchange LaunchAndCollectTrades(ThreadPool &threadPool, auto &trades, const TradeOptions &tradeOptions) {
  TradeResultPerExchange tradeResultPerExchange(static_cast<TradeResultPerExchange::size_type>(trades.size()));

  using ObjType = std::ranges::range_value_t<decltype(trades)>;

  threadPool.parallelTransform(
      trades, tradeResultPerExchange.begin(), [&tradeOptions](ObjType &exchangeAmountMarketsPath) {
        Exchange *exchange = exchangeAmountMarketsPath.exchange;
        const MonetaryAmount from = exchangeAmountMarketsPath.amount;
        const CurrencyCode toCurrency = exchangeAmountMarketsPath.currency;
        const auto &marketsPath = exchangeAmountMarketsPath.marketsPath;

        TradedAmounts tradedAmounts = exchange->apiPrivate().trade(from, toCurrency, tradeOptions, marketsPath);
        return std::make_pair(exchange, TradeResult(std::move(tradedAmounts), from));
      });
  return tradeResultPerExchange;
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

  auto fiats = QueryFiats(publicExchanges);

  return FilterConversionPaths(exchangeAmountPairVector, fromCurrency, toCurrency, marketsPerPublicExchange, fiats,
                               tradeOptions);
}

}  // namespace

TradeResultPerExchange ExchangesOrchestrator::trade(MonetaryAmount from, bool isPercentageTrade,
                                                    CurrencyCode toCurrency, ExchangeNameSpan privateExchangeNames,
                                                    const TradeOptions &tradeOptions) {
  if (privateExchangeNames.size() == 1 && !isPercentageTrade) {
    // In this special case we don't need to call the balance - call trade directly
    Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeNames.front());
    TradedAmounts tradedAmounts = exchange.apiPrivate().trade(from, toCurrency, tradeOptions);
    return {1, std::make_pair(&exchange, TradeResult(std::move(tradedAmounts), from))};
  }

  const CurrencyCode fromCurrency = from.currencyCode();

  ExchangeAmountMarketsPathVector exchangeAmountMarketsPathVector = CreateExchangeAmountMarketsPathVector(
      _exchangeRetriever, getBalance(privateExchangeNames), fromCurrency, toCurrency, tradeOptions);

  MonetaryAmount currentTotalAmount(0, fromCurrency);

  auto it = exchangeAmountMarketsPathVector.begin();
  if (!exchangeAmountMarketsPathVector.empty()) {
    // Sort exchanges from largest to lowest available amount (should be after filter on markets and conversion paths)
    std::ranges::stable_sort(exchangeAmountMarketsPathVector,
                             [](const auto &lhs, const auto &rhs) { return lhs.amount > rhs.amount; });

    // Locate the point where there is enough available amount to trade for this currency
    if (isPercentageTrade) {
      MonetaryAmount totalAvailableAmount =
          std::accumulate(exchangeAmountMarketsPathVector.begin(), exchangeAmountMarketsPathVector.end(),
                          currentTotalAmount, [](MonetaryAmount tot, const auto &tuple) { return tot + tuple.amount; });
      from = (totalAvailableAmount * from.toNeutral()) / 100;
    }
    for (auto endIt = exchangeAmountMarketsPathVector.end(); it != endIt && currentTotalAmount < from; ++it) {
      MonetaryAmount &amount = it->amount;
      if (currentTotalAmount + amount > from) {
        // Cap last amount such that total start trade on all exchanges reaches exactly 'startAmount'
        amount = from - currentTotalAmount;
      }
      currentTotalAmount += amount;
    }
  }

  if (currentTotalAmount == 0) {
    log::warn("No available {} to trade", fromCurrency);
  } else if (currentTotalAmount < from) {
    log::warn("Will trade {} < {} amount", currentTotalAmount, from);
  }

  /// We have enough total available amount. Launch all trades in parallel
  return LaunchAndCollectTrades(_threadPool,
                                std::span<ExchangeAmountMarkets>(exchangeAmountMarketsPathVector.begin(), it),
                                toCurrency, tradeOptions);
}

TradeResultPerExchange ExchangesOrchestrator::smartBuy(MonetaryAmount endAmount, ExchangeNameSpan privateExchangeNames,
                                                       const TradeOptions &tradeOptions) {
  const CurrencyCode toCurrency = endAmount.currencyCode();
  BalancePerExchange balancePerExchange = getBalance(privateExchangeNames);

  // Keep only exchanges which have some amount on at least one of the preferred payment currencies
  SmallVector<bool, kTypicalNbPrivateAccounts> exchangesWithSomePreferredPaymentCurrency(balancePerExchange.size());
  std::ranges::transform(
      balancePerExchange, exchangesWithSomePreferredPaymentCurrency.begin(), [](auto &exchangeBalancePair) {
        return std::ranges::any_of(exchangeBalancePair.first->exchangeConfig().asset.preferredPaymentCurrencies,
                                   [&](CurrencyCode cur) { return exchangeBalancePair.second.hasSome(cur); });
      });
  FilterVector(balancePerExchange, exchangesWithSomePreferredPaymentCurrency);

  ExchangeRetriever::PublicExchangesVec publicExchanges =
      SelectUniquePublicExchanges(_exchangeRetriever, balancePerExchange);

  MarketSetsPerPublicExchange marketsPerPublicExchange(publicExchanges.size());

  FixedCapacityVector<MarketOrderBookMap, kNbSupportedExchanges> marketOrderBooksPerPublicExchange(
      publicExchanges.size());

  auto fiats = QueryFiats(publicExchanges);

  ExchangeAmountToCurrencyToAmountVector trades;
  MonetaryAmount remEndAmount = endAmount;
  for (int nbSteps = 1;; ++nbSteps) {
    bool continuingHigherStepsPossible = false;
    const int nbTrades = static_cast<int>(trades.size());
    int publicExchangePos = -1;
    api::ExchangePublic *pExchangePublic = nullptr;
    for (auto &[pExchange, balance] : balancePerExchange) {
      if (pExchangePublic != &pExchange->apiPublic()) {
        pExchangePublic = &pExchange->apiPublic();
        ++publicExchangePos;
      }
      api::ExchangePublic &exchangePublic = *pExchangePublic;
      const auto &exchangeConfig = exchangePublic.exchangeConfig();
      if (nbSteps > 1 && !tradeOptions.isMultiTradeAllowed(exchangeConfig.query.multiTradeAllowedByDefault)) {
        continue;
      }
      auto &markets = marketsPerPublicExchange[publicExchangePos];
      auto &marketOrderBookMap = marketOrderBooksPerPublicExchange[publicExchangePos];
      for (CurrencyCode fromCurrency : exchangeConfig.asset.preferredPaymentCurrencies) {
        if (fromCurrency == toCurrency) {
          continue;
        }
        MonetaryAmount avAmount = balance.get(fromCurrency);
        if (avAmount > 0 &&
            std::none_of(trades.begin(), trades.begin() + nbTrades, [pExchange, fromCurrency](const auto &obj) {
              return obj.exchange == pExchange && obj.amount.currencyCode() == fromCurrency;
            })) {
          auto conversionPath = exchangePublic.findMarketsPath(fromCurrency, toCurrency, markets, fiats,
                                                               api::ExchangePublic::MarketPathMode::kStrict);
          const int nbConversions = static_cast<int>(conversionPath.size());
          if (nbConversions > nbSteps) {
            continuingHigherStepsPossible = true;
          } else if (nbConversions == nbSteps) {
            MonetaryAmount startAmount = avAmount;
            std::optional<MonetaryAmount> optEndAmount = exchangePublic.convert(
                startAmount, toCurrency, conversionPath, fiats, marketOrderBookMap, tradeOptions.priceOptions());
            if (optEndAmount) {
              trades.emplace_back(pExchange, startAmount, toCurrency, std::move(conversionPath), *optEndAmount);
            }
          }
        }
      }
    }
    // Sort exchanges from largest to lowest end amount
    std::stable_sort(trades.begin() + nbTrades, trades.end(),
                     [](const auto &lhs, const auto &rhs) { return lhs.endAmount > rhs.endAmount; });
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

  return LaunchAndCollectTrades(_threadPool, trades, tradeOptions);
}

TradeResultPerExchange ExchangesOrchestrator::smartSell(MonetaryAmount startAmount, bool isPercentageTrade,
                                                        ExchangeNameSpan privateExchangeNames,
                                                        const TradeOptions &tradeOptions) {
  const CurrencyCode fromCurrency = startAmount.currencyCode();
  // Retrieve amount per start amount currency for each exchange
  ExchangeAmountPairVector exchangeAmountPairVector =
      ComputeExchangeAmountPairVector(fromCurrency, getBalance(privateExchangeNames));

  ExchangeAmountToCurrencyVector trades;
  MonetaryAmount remStartAmount = startAmount;
  if (!exchangeAmountPairVector.empty()) {
    // Sort exchanges from largest to lowest available amount
    std::ranges::stable_sort(exchangeAmountPairVector, std::greater{},
                             [](const auto &exchangeAmount) { return exchangeAmount.second; });

    ExchangeRetriever::PublicExchangesVec publicExchanges =
        SelectUniquePublicExchanges(_exchangeRetriever, exchangeAmountPairVector, false);  // unsorted

    MarketSetsPerPublicExchange marketsPerPublicExchange(publicExchanges.size());

    // As we want to sort Exchanges by largest to smallest amount, we cannot directly map MarketSets per Exchange.
    // That's why we need to keep pointers to MarketSets ordered by exchanges
    MarketSetsPtrPerExchange marketSetsPtrPerExchange =
        MapMarketSetsPtrInExchangesOrder(exchangeAmountPairVector, publicExchanges, marketsPerPublicExchange);

    auto fiats = QueryFiats(publicExchanges);

    if (isPercentageTrade) {
      MonetaryAmount totalAvailableAmount = std::accumulate(
          exchangeAmountPairVector.begin(), exchangeAmountPairVector.end(), MonetaryAmount(0, fromCurrency),
          [](MonetaryAmount tot, const auto &tuple) { return tot + std::get<1>(tuple); });
      startAmount = (totalAvailableAmount * startAmount.toNeutral()) / 100;
      remStartAmount = startAmount;
    }

    // check from which exchanges we can start trades, minimizing number of steps per trade
    for (int nbSteps = 1;; ++nbSteps) {
      bool continuingHigherStepsPossible = false;
      int exchangePos = 0;
      for (auto &[pExchange, avAmount] : exchangeAmountPairVector) {
        if (avAmount == 0 ||  // It can be set to 0 in below code
            (nbSteps > 1 &&
             !tradeOptions.isMultiTradeAllowed(pExchange->exchangeConfig().query.multiTradeAllowedByDefault))) {
          ++exchangePos;
          continue;
        }
        MarketSet &markets = *marketSetsPtrPerExchange[exchangePos];
        for (CurrencyCode toCurrency : pExchange->exchangeConfig().asset.preferredPaymentCurrencies) {
          if (fromCurrency == toCurrency) {
            continue;
          }
          MarketsPath path = pExchange->apiPublic().findMarketsPath(fromCurrency, toCurrency, markets, fiats,
                                                                    api::ExchangePublic::MarketPathMode::kStrict);
          if (std::cmp_greater(path.size(), nbSteps)) {
            continuingHigherStepsPossible = true;
          } else if (std::cmp_equal(path.size(), nbSteps)) {
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

  return LaunchAndCollectTrades(_threadPool, trades, tradeOptions);
}

TradedAmountsVectorWithFinalAmountPerExchange ExchangesOrchestrator::dustSweeper(ExchangeNameSpan privateExchangeNames,
                                                                                 CurrencyCode currencyCode) {
  log::info("Query {} dust sweeper from {}", currencyCode, ConstructAccumulatedExchangeNames(privateExchangeNames));

  ExchangeRetriever::SelectedExchanges selExchanges = _exchangeRetriever.select(
      ExchangeRetriever::Order::kInitial, privateExchangeNames, ExchangeRetriever::Filter::kWithAccountWhenEmpty);

  TradedAmountsVectorWithFinalAmountPerExchange ret(selExchanges.size());
  _threadPool.parallelTransform(selExchanges, ret.begin(), [currencyCode](Exchange *exchange) {
    return std::make_pair(static_cast<const Exchange *>(exchange),
                          exchange->apiPrivate().queryDustSweeper(currencyCode));
  });

  return ret;
}

DeliveredWithdrawInfoWithExchanges ExchangesOrchestrator::withdraw(MonetaryAmount grossAmount,
                                                                   bool isPercentageWithdraw,
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
  const std::array exchangePair = {std::addressof(fromExchange), std::addressof(toExchange)};
  if (exchangePair.front() == exchangePair.back()) {
    throw exception("Cannot withdraw to the same account");
  }

  std::array<CurrencyExchangeFlatSet, 2> currencyExchangeSets;
  _threadPool.parallelTransform(exchangePair, currencyExchangeSets.begin(),
                                [](Exchange *exchange) { return exchange->queryTradableCurrencies(); });

  DeliveredWithdrawInfoWithExchanges ret{{&fromExchange, &toExchange}, DeliveredWithdrawInfo{}};

  if (!fromExchange.canWithdraw(currencyCode, currencyExchangeSets.front())) {
    string errMsg("It's currently not possible to withdraw ");
    currencyCode.appendStrTo(errMsg);
    errMsg.append(" from ").append(fromPrivateExchangeName.str());
    log::error(errMsg);
    ret.second = DeliveredWithdrawInfo(std::move(errMsg));
    return ret;
  }
  if (!toExchange.canDeposit(currencyCode, currencyExchangeSets.back())) {
    string errMsg("It's currently not possible to deposit ");
    currencyCode.appendStrTo(errMsg);
    errMsg.append(" to ").append(fromPrivateExchangeName.str());
    log::error(errMsg);
    ret.second = DeliveredWithdrawInfo(std::move(errMsg));
    return ret;
  }

  if (isPercentageWithdraw) {
    MonetaryAmount avAmount = fromExchange.apiPrivate().getAccountBalance().get(currencyCode);
    grossAmount = (avAmount * grossAmount.toNeutral()) / 100;
  }
  ret.second = fromExchange.apiPrivate().withdraw(grossAmount, toExchange.apiPrivate(), withdrawOptions);
  return ret;
}

MonetaryAmountByCurrencySetPerExchange ExchangesOrchestrator::getWithdrawFees(CurrencyCode currencyCode,
                                                                              ExchangeNameSpan exchangeNames) {
  if (currencyCode.isNeutral()) {
    log::info("Withdraw fees for {}", ConstructAccumulatedExchangeNames(exchangeNames));
  } else {
    log::info("{} withdraw fees for {}", currencyCode, ConstructAccumulatedExchangeNames(exchangeNames));
  }

  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames, true);

  MonetaryAmountByCurrencySetPerExchange withdrawFeesPerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, withdrawFeesPerExchange.begin(), [currencyCode](Exchange *exchange) {
    MonetaryAmountByCurrencySet withdrawFees;
    if (currencyCode.isNeutral()) {
      withdrawFees = exchange->queryWithdrawalFees();
    } else {
      std::optional<MonetaryAmount> optWithdrawFee = exchange->queryWithdrawalFee(currencyCode);
      if (optWithdrawFee) {
        withdrawFees.insert(*optWithdrawFee);
      }
    }
    return std::make_pair(exchange, std::move(withdrawFees));
  });
  return withdrawFeesPerExchange;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLast24hTradedVolumePerExchange(Market mk,
                                                                                   ExchangeNameSpan exchangeNames) {
  log::info("Query last 24h traded volume of {} pair on {}", mk, ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, tradedVolumePerExchange.begin(), [mk](Exchange *exchange) {
    return std::make_pair(exchange, exchange->queryLast24hVolume(mk));
  });
  return tradedVolumePerExchange;
}

TradesPerExchange ExchangesOrchestrator::getLastTradesPerExchange(Market mk, ExchangeNameSpan exchangeNames,
                                                                  std::optional<int> depth) {
  const auto nbLastTrades = depth.value_or(api::ExchangePublic::kNbLastTradesDefault);
  log::info("Query {} last trades on {} volume from {}", nbLastTrades, mk,
            ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  TradesPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [mk, nbLastTrades](Exchange *exchange) {
    return std::make_pair(static_cast<const Exchange *>(exchange), exchange->getLastTrades(mk, nbLastTrades));
  });

  return ret;
}

MonetaryAmountPerExchange ExchangesOrchestrator::getLastPricePerExchange(Market mk, ExchangeNameSpan exchangeNames) {
  log::info("Query last price from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(mk, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, lastPricePerExchange.begin(), [mk](Exchange *exchange) {
    return std::make_pair(exchange, exchange->queryLastPrice(mk));
  });
  return lastPricePerExchange;
}

MarketDataPerExchange ExchangesOrchestrator::getMarketDataPerExchange(
    std::span<const Market> marketPerPublicExchange, std::span<const ExchangeNameEnum> exchangeNameEnums) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNameEnums);

  std::array<bool, kNbSupportedExchanges> isMarketTradable;

  _threadPool.parallelTransform(selectedExchanges, isMarketTradable.begin(),
                                [&marketPerPublicExchange](Exchange *exchange) {
                                  Market market = marketPerPublicExchange[exchange->publicExchangePos()];
                                  return market.isDefined() && exchange->queryTradableMarkets().contains(market);
                                });

  FilterVector(selectedExchanges, isMarketTradable);

  MarketDataPerExchange ret(selectedExchanges.size());
  _threadPool.parallelTransform(selectedExchanges, ret.begin(), [&marketPerPublicExchange](Exchange *exchange) {
    // Call order book and last trades sequentially for this exchange
    Market market = marketPerPublicExchange[exchange->publicExchangePos()];

    // Use local variables to ensure deterministic order of api calls (orderbook then last trades).
    // The order of api calls itself is not important, but we want to keep the same order for repetitive calls.
    // Indeed in C++, the order of parameter evaluation is undefined, and we don't want that it changes between two
    // calls even if it's very unlikely.
    auto orderBook = exchange->getOrderBook(market);
    auto lastTrades = exchange->getLastTrades(market);

    return std::make_pair(exchange, std::make_pair(std::move(orderBook), std::move(lastTrades)));
  });
  return ret;
}

MarketTimestampSetsPerExchange ExchangesOrchestrator::pullAvailableMarketsForReplay(TimeWindow timeWindow,
                                                                                    ExchangeNameSpan exchangeNames) {
  log::info("Query available markets for replay from {} within {}", ConstructAccumulatedExchangeNames(exchangeNames),
            timeWindow);
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  MarketTimestampSetsPerExchange marketTimestampSetsPerExchange(selectedExchanges.size());
  _threadPool.parallelTransform(
      selectedExchanges, marketTimestampSetsPerExchange.begin(), [timeWindow](Exchange *exchange) {
        auto &apiPublic = exchange->apiPublic();

        auto orderBooks = apiPublic.pullMarketOrderBooksMarkets(timeWindow);
        auto trades = apiPublic.pullTradeMarkets(timeWindow);

        return std::make_pair(exchange, MarketTimestampSets{std::move(orderBooks), std::move(trades)});
      });
  return marketTimestampSetsPerExchange;
}

MarketTradeRangeStatsPerExchange ExchangesOrchestrator::traderConsumeRange(
    const ReplayOptions &replayOptions, TimeWindow subTimeWindow, std::span<MarketTraderEngine> marketTraderEngines,
    ExchangeNameEnumSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  MarketTradeRangeStatsPerExchange tradeRangeResultsPerExchange(selectedExchanges.size());

  _threadPool.parallelTransform(
      selectedExchanges, marketTraderEngines, tradeRangeResultsPerExchange.begin(),
      [subTimeWindow, &replayOptions](Exchange *exchange, MarketTraderEngine &marketTraderEngine) {
        Market market = marketTraderEngine.market();
        auto &apiPublic = exchange->apiPublic();

        auto marketOrderBooks = apiPublic.pullMarketOrderBooksForReplay(market, subTimeWindow);
        auto publicTrades = apiPublic.pullTradesForReplay(market, subTimeWindow);

        TradeRangeStats tradeRangeStats;

        switch (replayOptions.replayMode()) {
          case ReplayOptions::ReplayMode::kValidateOnly:
            tradeRangeStats = marketTraderEngine.validateRange(std::move(marketOrderBooks), std::move(publicTrades));
            break;
          case ReplayOptions::ReplayMode::kCheckedLaunchAlgorithm:
            tradeRangeStats = marketTraderEngine.validateRange(marketOrderBooks, publicTrades);
            marketTraderEngine.tradeRange(std::move(marketOrderBooks), std::move(publicTrades));
            break;
          case ReplayOptions::ReplayMode::kUncheckedLaunchAlgorithm:
            tradeRangeStats = marketTraderEngine.tradeRange(std::move(marketOrderBooks), std::move(publicTrades));
            break;
          default:
            break;
        }

        return std::make_pair(exchange, std::move(tradeRangeStats));
      });

  return tradeRangeResultsPerExchange;
}

MarketTradingGlobalResultPerExchange ExchangesOrchestrator::getMarketTraderResultPerExchange(
    std::span<MarketTraderEngine> marketTraderEngines, MarketTradeRangeStatsPerExchange &&tradeRangeStatsPerExchange,
    ExchangeNameEnumSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  if (selectedExchanges.size() != tradeRangeStatsPerExchange.size()) {
    throw exception("Inconsistent selected exchange sizes");
  }

  MarketTradingResultPerExchange marketTradingResultPerExchange(selectedExchanges.size());

  _threadPool.parallelTransform(selectedExchanges, marketTraderEngines, marketTradingResultPerExchange.begin(),
                                [](const Exchange *exchange, MarketTraderEngine &marketTraderEngine) {
                                  return std::make_pair(exchange, marketTraderEngine.finalizeAndComputeResult());
                                });

  MarketTradingGlobalResultPerExchange marketTradingGlobalResultPerExchange(selectedExchanges.size());
  std::ranges::transform(
      marketTradingResultPerExchange, tradeRangeStatsPerExchange, marketTradingGlobalResultPerExchange.begin(),
      [](auto &exchangeMarketTradingResult, auto &exchangeTradeRangeStats) {
        return std::make_pair(exchangeMarketTradingResult.first,
                              MarketTradingGlobalResult{std::move(exchangeMarketTradingResult.second),
                                                        std::move(exchangeTradeRangeStats.second)});
      });

  return marketTradingGlobalResultPerExchange;
}

}  // namespace cct
