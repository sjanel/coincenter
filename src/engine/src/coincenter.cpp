#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <execution>
#include <thread>

#include "abstractmetricgateway.hpp"
#include "cct_exception.hpp"
#include "cct_smallvector.hpp"
#include "coincenteroptions.hpp"
#include "coincenterparsedoptions.hpp"
#include "printqueryresults.hpp"
#include "stringoptionparser.hpp"

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

}  // namespace

Coincenter::Coincenter(const PublicExchangeNames &exchangesWithoutSecrets, bool allExchangesWithoutSecrets,
                       settings::RunMode runMode, std::string_view dataDir, const MonitoringInfo &monitoringInfo)
    : _coincenterInfo(runMode, dataDir, monitoringInfo),
      _cryptowatchAPI(_coincenterInfo, runMode),
      _fiatConverter(_coincenterInfo, std::chrono::hours(8)),
      _apiKeyProvider(dataDir, exchangesWithoutSecrets, allExchangesWithoutSecrets, runMode),
      _metricsExporter(_coincenterInfo.metricGatewayPtr()),
      _exchangePool(_coincenterInfo, _fiatConverter, _cryptowatchAPI, _apiKeyProvider),
      _exchangeRetriever(_exchangePool.exchanges()),
      _cexchangeRetriever(_exchangePool.exchanges()) {}

void Coincenter::process(const CoincenterParsedOptions &opts) {
  processWriteRequests(opts);
  const int nbRepeats = opts.repeats;
  for (int repeatPos = 0; repeatPos != nbRepeats; ++repeatPos) {
    if (repeatPos != 0) {
      std::this_thread::sleep_for(opts.repeat_time);
    }
    if (nbRepeats != 1) {
      if (nbRepeats == -1) {
        log::info("Processing read request {}/{}", repeatPos + 1, "\u221E");  // unicode char for infinity sign
      } else {
        log::info("Processing read request {}/{}", repeatPos + 1, nbRepeats);
      }
    }

    processReadRequests(opts);
  }
}

ExchangeTickerMaps Coincenter::getTickerInformation(ExchangeNameSpan exchangeNames) {
  log::info("Ticker information for {}", ConstructAccumulatedExchangeNames(exchangeNames));

  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);

  ExchangeTickerMaps ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [](Exchange *e) { return std::make_pair(e, e->apiPublic().queryAllApproximatedOrderBooks(1)); });

  _metricsExporter.exportTickerMetrics(ret);

  return ret;
}

MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(Market m, ExchangeNameSpan exchangeNames,
                                                               CurrencyCode equiCurrencyCode,
                                                               std::optional<int> depth) {
  log::info("Order book of {} on {} requested{}{}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames),
            equiCurrencyCode != CurrencyCode() ? " with equi currency " : "",
            equiCurrencyCode != CurrencyCode() ? equiCurrencyCode.str() : "");
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradable;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), isMarketTradable.begin(),
                 [m](Exchange *e) { return e->apiPublic().queryTradableMarkets().contains(m); });

  FilterVector(selectedExchanges, isMarketTradable);

  MarketOrderBookConversionRates ret(selectedExchanges.size());
  auto marketOrderBooksFunc = [m, equiCurrencyCode, depth](Exchange *e) {
    api::ExchangePublic &publicExchange = e->apiPublic();
    std::optional<MonetaryAmount> optConversionRate =
        equiCurrencyCode == CurrencyCode::kNeutral
            ? std::nullopt
            : publicExchange.convertAtAveragePrice(MonetaryAmount(1, m.quote()), equiCurrencyCode);
    MarketOrderBook marketOrderBook(depth ? publicExchange.queryOrderBook(m, *depth)
                                          : publicExchange.queryOrderBook(m));
    if (!optConversionRate && equiCurrencyCode != CurrencyCode::kNeutral) {
      log::warn("Unable to convert {} into {} on {}", marketOrderBook.market().quote().str(), equiCurrencyCode.str(),
                e->name());
    }
    return std::make_tuple(e->name(), std::move(marketOrderBook), optConversionRate);
  };
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 marketOrderBooksFunc);

  _metricsExporter.exportOrderbookMetrics(m, ret);
  return ret;
}

BalancePerExchange Coincenter::getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                          CurrencyCode equiCurrency) {
  log::info("Query balance from {}", ConstructAccumulatedExchangeNames(privateExchangeNames));
  std::optional<CurrencyCode> optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
  if (optEquiCur) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", optEquiCur->str(), equiCurrency.str());
    equiCurrency = *optEquiCur;
  }

  ExchangeRetriever::SelectedExchanges balanceExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts> balancePortfolios(balanceExchanges.size());
  std::transform(std::execution::par, balanceExchanges.begin(), balanceExchanges.end(), balancePortfolios.begin(),
                 [equiCurrency](Exchange *e) { return e->apiPrivate().getAccountBalance(equiCurrency); });

  BalancePerExchange ret;
  ret.reserve(balanceExchanges.size());
  std::transform(balanceExchanges.begin(), balanceExchanges.end(), std::make_move_iterator(balancePortfolios.begin()),
                 std::back_inserter(ret),
                 [](const Exchange *e, BalancePortfolio &&b) { return std::make_pair(e, std::move(b)); });

  _metricsExporter.exportBalanceMetrics(ret, equiCurrency);

  return ret;
}

WalletPerExchange Coincenter::getDepositInfo(std::span<const PrivateExchangeName> privateExchangeNames,
                                             CurrencyCode depositCurrency) {
  log::info("Query {} deposit information from {}", depositCurrency.str(),
            ConstructAccumulatedExchangeNames(privateExchangeNames));
  ExchangeRetriever::SelectedExchanges depositInfoExchanges =
      _exchangeRetriever.select(ExchangeRetriever::Order::kInitial, privateExchangeNames);

  /// Filter only on exchanges with can receive given currency
  SmallVector<bool, kTypicalNbPrivateAccounts> canDepositCurrency(depositInfoExchanges.size());
  // Do not call in parallel here because tradable currencies service could be queried from several identical public
  // exchanges (when there are several accounts for one exchange)
  std::transform(
      depositInfoExchanges.begin(), depositInfoExchanges.end(), canDepositCurrency.begin(),
      [depositCurrency](Exchange *e) { return e->apiPrivate().queryTradableCurrencies().contains(depositCurrency); });

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

ConversionPathPerExchange Coincenter::getConversionPaths(Market m, ExchangeNameSpan exchangeNames) {
  log::info("Query {} conversion path from {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  ConversionPathPerExchange conversionPathPerExchange(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), conversionPathPerExchange.begin(),
      [m](Exchange *e) { return std::make_pair(e, e->apiPublic().findFastestConversionPath(m.base(), m.quote())); });

  return conversionPathPerExchange;
}

MarketsPerExchange Coincenter::getMarketsPerExchange(CurrencyCode cur, ExchangeNameSpan exchangeNames) {
  log::info("Query markets from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  MarketsPerExchange marketsPerExchange(selectedExchanges.size());
  auto marketsWithCur = [cur](Exchange *e) {
    api::ExchangePublic::MarketSet markets = e->apiPublic().queryTradableMarkets();
    api::ExchangePublic::MarketSet ret;
    std::copy_if(markets.begin(), markets.end(), std::inserter(ret, ret.end()),
                 [cur](Market m) { return m.canTrade(cur); });
    return std::make_pair(e, std::move(ret));
  };
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), marketsPerExchange.begin(),
                 marketsWithCur);
  return marketsPerExchange;
}

UniquePublicSelectedExchanges Coincenter::getExchangesTradingCurrency(CurrencyCode currencyCode,
                                                                      ExchangeNameSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isCurrencyTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isCurrencyTradablePerExchange.begin(),
                 [currencyCode](Exchange *e) { return e->queryTradableCurrencies().contains(currencyCode); });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isCurrencyTradablePerExchange);
  return selectedExchanges;
}

UniquePublicSelectedExchanges Coincenter::getExchangesTradingMarket(Market m, ExchangeNameSpan exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isMarketTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isMarketTradablePerExchange.begin(),
                 [m](Exchange *e) { return e->apiPublic().queryTradableMarkets().contains(m); });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isMarketTradablePerExchange);

  return selectedExchanges;
}

MonetaryAmount Coincenter::trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                                 const PrivateExchangeName &privateExchangeName, const TradeOptions &tradeOptions) {
  Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeName);
  return exchange.apiPrivate().trade(startAmount, toCurrency, tradeOptions);
}

MonetaryAmount Coincenter::tradeAll(CurrencyCode fromCurrency, CurrencyCode toCurrency,
                                    const PrivateExchangeName &privateExchangeName, const TradeOptions &tradeOptions) {
  Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeName);
  MonetaryAmount startAmount = exchange.apiPrivate().getAccountBalance().get(fromCurrency);
  return exchange.apiPrivate().trade(startAmount, toCurrency, tradeOptions);
}

WithdrawInfo Coincenter::withdraw(MonetaryAmount grossAmount, const PrivateExchangeName &fromPrivateExchangeName,
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

WithdrawFeePerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode, ExchangeNameSpan exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames);

  WithdrawFeePerExchange withdrawFeePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 withdrawFeePerExchange.begin(),
                 [currencyCode](Exchange *e) { return std::make_pair(e, e->queryWithdrawalFee(currencyCode)); });
  return withdrawFeePerExchange;
}

MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(Market m, ExchangeNameSpan exchangeNames) {
  log::info("Query last 24h traded volume of {} pair on {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 tradedVolumePerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->apiPublic().queryLast24hVolume(m)); });
  return tradedVolumePerExchange;
}

LastTradesPerExchange Coincenter::getLastTradesPerExchange(Market m, ExchangeNameSpan exchangeNames, int nbLastTrades) {
  log::info("Query {} last trades on {} volume from {}", nbLastTrades, m.str(),
            ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  LastTradesPerExchange ret(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), ret.begin(),
                 [m, nbLastTrades](Exchange *e) {
                   return std::make_pair(static_cast<const Exchange *>(e),
                                         e->apiPublic().queryLastTrades(m, nbLastTrades));
                 });

  _metricsExporter.exportLastTradesMetrics(m, ret);
  return ret;
}

MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(Market m, ExchangeNameSpan exchangeNames) {
  log::info("Query last price from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), lastPricePerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->apiPublic().queryLastPrice(m)); });
  return lastPricePerExchange;
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  auto exchanges = _exchangePool.exchanges();
  std::for_each(exchanges.begin(), exchanges.end(), [](const Exchange &e) { e.updateCacheFile(); });
}

void Coincenter::processReadRequests(const CoincenterParsedOptions &opts) {
  if (opts.marketsCurrency != CurrencyCode()) {
    MarketsPerExchange marketsPerExchange = getMarketsPerExchange(opts.marketsCurrency, opts.marketsExchanges);
    PrintMarkets(opts.marketsCurrency, marketsPerExchange);
  }

  if (opts.tickerForAll || !opts.tickerExchanges.empty()) {
    ExchangeTickerMaps exchangeTickerMaps = getTickerInformation(opts.tickerExchanges);
    PrintTickerInformation(exchangeTickerMaps);
  }

  if (opts.marketForOrderBook != Market()) {
    std::optional<int> depth;
    if (opts.orderbookDepth != 0) {
      depth = opts.orderbookDepth;
    }
    MarketOrderBookConversionRates marketOrderBooksConversionRates =
        getMarketOrderBooks(opts.marketForOrderBook, opts.orderBookExchanges, opts.orderbookCur, depth);
    PrintMarketOrderBooks(marketOrderBooksConversionRates);
  }

  if (opts.marketForConversionPath != Market()) {
    ConversionPathPerExchange conversionPathPerExchange =
        getConversionPaths(opts.marketForConversionPath, opts.conversionPathExchanges);
    PrintConversionPath(opts.marketForConversionPath, conversionPathPerExchange);
  }

  if (opts.balanceForAll || !opts.balancePrivateExchanges.empty()) {
    BalancePerExchange balancePerExchange = getBalance(opts.balancePrivateExchanges, opts.balanceCurrencyCode);
    PrintBalance(balancePerExchange);
  }

  if (opts.depositCurrency != CurrencyCode()) {
    WalletPerExchange walletPerExchange = getDepositInfo(opts.depositInfoPrivateExchanges, opts.depositCurrency);

    PrintDepositInfo(opts.depositCurrency, walletPerExchange);
  }

  if (opts.withdrawFeeCur != CurrencyCode()) {
    auto withdrawFeesPerExchange = getWithdrawFees(opts.withdrawFeeCur, opts.withdrawFeeExchanges);
    PrintWithdrawFees(withdrawFeesPerExchange);
  }

  if (opts.tradedVolumeMarket != Market()) {
    MonetaryAmountPerExchange tradedVolumePerExchange =
        getLast24hTradedVolumePerExchange(opts.tradedVolumeMarket, opts.tradedVolumeExchanges);
    PrintLast24hTradedVolume(opts.tradedVolumeMarket, tradedVolumePerExchange);
  }

  if (opts.lastTradesMarket != Market()) {
    LastTradesPerExchange lastTradesPerExchange =
        getLastTradesPerExchange(opts.lastTradesMarket, opts.lastTradesExchanges, opts.nbLastTrades);
    PrintLastTrades(opts.lastTradesMarket, lastTradesPerExchange);
  }

  if (opts.lastPriceMarket != Market()) {
    MonetaryAmountPerExchange lastPricePerExchange =
        getLastPricePerExchange(opts.lastPriceMarket, opts.lastPriceExchanges);
    PrintLastPrice(opts.lastPriceMarket, lastPricePerExchange);
  }
}

void Coincenter::processWriteRequests(const CoincenterParsedOptions &opts) {
  if (opts.fromTradeCurrency != CurrencyCode()) {
    tradeAll(opts.fromTradeCurrency, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
  }

  if (!opts.startTradeAmount.isZero()) {
    MonetaryAmount startAmount = opts.startTradeAmount;
    trade(startAmount, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
  }

  if (!opts.amountToWithdraw.isZero()) {
    withdraw(opts.amountToWithdraw, opts.withdrawFromExchangeName, opts.withdrawToExchangeName);
  }
}

}  // namespace cct
