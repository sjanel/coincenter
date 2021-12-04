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

ExchangeTickerMaps Coincenter::getTickerInformation(std::span<const ExchangeName> exchangeNames) {
  ExchangeTickerMaps ret(_exchangeRetriever.selectPublicExchanges(exchangeNames), MarketOrderBookMaps());
  ret.second.resize(ret.first.size());
  std::transform(std::execution::par, ret.first.begin(), ret.first.end(), ret.second.begin(),
                 [](api::ExchangePublic *e) { return e->queryAllApproximatedOrderBooks(1); });

  if (_coincenterInfo.useMonitoring() && !ret.second.empty()) {
    exportTickerMetrics(ret.first, ret.second);
  }
  return ret;
}

MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(Market m, std::span<const ExchangeName> exchangeNames,
                                                               CurrencyCode equiCurrencyCode,
                                                               std::optional<int> depth) {
  MarketOrderBookConversionRates ret;
  for (api::ExchangePublic *e : _exchangeRetriever.selectPublicExchanges(exchangeNames)) {
    // Do not check if market exists when exchange names are specified to save API call
    if (!exchangeNames.empty() || e->queryTradableMarkets().contains(m)) {
      std::optional<MonetaryAmount> optConversionRate =
          equiCurrencyCode == CurrencyCode::kNeutral
              ? std::nullopt
              : e->convertAtAveragePrice(MonetaryAmount(1, m.quote()), equiCurrencyCode);
      log::info("Order book of {} on {} requested{}{}", m.str(), e->name(),
                optConversionRate ? " with conversion rate " : "", optConversionRate ? optConversionRate->str() : "");
      MarketOrderBook marketOrderBook(depth ? e->queryOrderBook(m, *depth) : e->queryOrderBook(m));
      ret.emplace_back(e->name(), std::move(marketOrderBook), optConversionRate);
    }
  }
  if (_coincenterInfo.useMonitoring() && !ret.empty()) {
    exportOrderbookMetrics(m, ret);
  }
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

  if (_coincenterInfo.useMonitoring()) {
    exportBalanceMetrics(ret, equiCurrency);
  }

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

ConversionPathPerExchange Coincenter::getConversionPaths(Market m, std::span<const ExchangeName> exchangeNames) {
  log::info("Query {} conversion path from {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  ConversionPathPerExchange conversionPathPerExchange(selectedExchanges.size());
  std::transform(
      std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), conversionPathPerExchange.begin(),
      [m](Exchange *e) { return std::make_pair(e, e->apiPublic().findFastestConversionPath(m.base(), m.quote())); });

  return conversionPathPerExchange;
}

MarketsPerExchange Coincenter::getMarketsPerExchange(CurrencyCode cur, std::span<const ExchangeName> exchangeNames) {
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
                                                                      std::span<const ExchangeName> exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = _exchangeRetriever.selectOneAccount(exchangeNames);
  std::array<bool, kNbSupportedExchanges> isCurrencyTradablePerExchange;
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isCurrencyTradablePerExchange.begin(),
                 [currencyCode](Exchange *e) { return e->queryTradableCurrencies().contains(currencyCode); });

  // Erases Exchanges which do not propose asked currency
  FilterVector(selectedExchanges, isCurrencyTradablePerExchange);
  return selectedExchanges;
}

UniquePublicSelectedExchanges Coincenter::getExchangesTradingMarket(Market m,
                                                                    std::span<const ExchangeName> exchangeNames) {
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

WithdrawFeePerExchange Coincenter::getWithdrawFees(CurrencyCode currencyCode,
                                                   std::span<const ExchangeName> exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames);

  WithdrawFeePerExchange withdrawFeePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 withdrawFeePerExchange.begin(),
                 [currencyCode](Exchange *e) { return std::make_pair(e, e->queryWithdrawalFee(currencyCode)); });
  return withdrawFeePerExchange;
}

MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(Market m,
                                                                        std::span<const ExchangeName> exchangeNames) {
  log::info("Query last 24h traded volume of {} pair on {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 tradedVolumePerExchange.begin(),
                 [m](Exchange *e) { return std::make_pair(e, e->apiPublic().queryLast24hVolume(m)); });
  return tradedVolumePerExchange;
}

LastTradesPerExchange Coincenter::getLastTradesPerExchange(Market m, std::span<const ExchangeName> exchangeNames,
                                                           int nbLastTrades) {
  log::info("Query {} last trades on {} volume from {}", nbLastTrades, m.str(),
            ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  LastTradesPerExchange lastTradesPerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), lastTradesPerExchange.begin(),
                 [m, nbLastTrades](Exchange *e) {
                   return std::make_pair(static_cast<const Exchange *>(e),
                                         e->apiPublic().queryLastTrades(m, nbLastTrades));
                 });
  return lastTradesPerExchange;
}

MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(Market m, std::span<const ExchangeName> exchangeNames) {
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
    log::info("Ticker information for {}", ConstructAccumulatedExchangeNames(opts.tickerExchanges));
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
    PrintMarketOrderBooks(marketOrderBooksConversionRates, opts.orderbookCur);
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
  // Trade all
  if (opts.fromTradeCurrency != CurrencyCode()) {
    tradeAll(opts.fromTradeCurrency, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
  }

  // Trade
  if (!opts.startTradeAmount.isZero()) {
    MonetaryAmount startAmount = opts.startTradeAmount;
    trade(startAmount, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
  }

  if (!opts.amountToWithdraw.isZero()) {
    log::info("Withdraw gross {} from {} to {} requested", opts.amountToWithdraw.str(),
              opts.withdrawFromExchangeName.str(), opts.withdrawToExchangeName.str());
    withdraw(opts.amountToWithdraw, opts.withdrawFromExchangeName, opts.withdrawToExchangeName);
  }
}

void Coincenter::exportBalanceMetrics(const BalancePerExchange &balancePerExchange, CurrencyCode equiCurrency) const {
  MetricKey key("metric_name=available_balance,metric_help=Available balance in the exchange account");
  auto &metricGateway = _coincenterInfo.metricGateway();
  for (const auto &[exchangePtr, balancePortfolio] : balancePerExchange) {
    const Exchange &exchange = *exchangePtr;
    key.set("exchange", exchange.name());
    key.set("account", exchange.keyName());
    key.set("total", "no");
    MonetaryAmount totalEquiAmount(0, equiCurrency);
    for (BalancePortfolio::MonetaryAmountWithEquivalent amountWithEqui : balancePortfolio) {
      key.set("currency", amountWithEqui.amount.currencyStr());
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, amountWithEqui.amount.toDouble());
      if (!equiCurrency.isNeutral()) {
        totalEquiAmount += amountWithEqui.equi;
      }
    }
    if (!equiCurrency.isNeutral()) {
      key.set("total", "yes");
      key.set("currency", equiCurrency.str());
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, totalEquiAmount.toDouble());
    }
  }
}

void Coincenter::exportTickerMetrics(std::span<api::ExchangePublic *> exchanges,
                                     const MarketOrderBookMaps &marketOrderBookMaps) const {
  MetricKey key;
  const int nbExchanges = static_cast<int>(exchanges.size());
  auto &metricGateway = _coincenterInfo.metricGateway();
  for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
    const api::ExchangePublic &e = *exchanges[exchangePos];
    key.set("metric_name", "limit_price");
    key.set("metric_help", "Best bids and asks prices");
    key.set("exchange", e.name());
    for (const auto &[m, marketOrderbook] : marketOrderBookMaps[exchangePos]) {
      key.set("market", m.assetsPairStr('-', true));
      key.set("side", "ask");
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderbook.lowestAskPrice().toDouble());
      key.set("side", "bid");
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderbook.highestBidPrice().toDouble());
    }
    key.set("metric_name", "limit_volume");
    key.set("metric_help", "Best bids and asks volumes");
    for (const auto &[m, marketOrderbook] : marketOrderBookMaps[exchangePos]) {
      key.set("market", m.assetsPairStr('-', true));
      key.set("side", "ask");
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderbook.amountAtAskPrice().toDouble());
      key.set("side", "bid");
      metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderbook.amountAtBidPrice().toDouble());
    }
  }
}

void Coincenter::exportOrderbookMetrics(Market m,
                                        const MarketOrderBookConversionRates &marketOrderBookConversionRates) const {
  MetricKey key("metric_name=limit_pri,metric_help=Best bids and asks prices");
  string marketLowerCase = m.assetsPairStr('-', true);
  auto &metricGateway = _coincenterInfo.metricGateway();
  key.append("market", marketLowerCase);
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("exchange", exchangeName);
    key.set("side", "ask");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.lowestAskPrice().toDouble());
    key.set("side", "bid");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.highestBidPrice().toDouble());
  }
  key.set("metric_name", "limit_vol");
  key.set("metric_help", "Best bids and asks volumes");
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("exchange", exchangeName);
    key.set("side", "ask");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.amountAtAskPrice().toDouble());
    key.set("side", "bid");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.amountAtBidPrice().toDouble());
  }
}

}  // namespace cct
