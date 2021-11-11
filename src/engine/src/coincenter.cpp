#include "coincenter.hpp"

#include <algorithm>
#include <chrono>
#include <execution>
#include <span>

#include "abstractmetricgateway.hpp"
#include "cct_exception.hpp"
#include "cct_fixedcapacityvector.hpp"
#include "cct_smallvector.hpp"
#include "cct_time_helpers.hpp"
#include "cct_variadictable.hpp"
#include "coincenteroptions.hpp"
#include "coincenterparsedoptions.hpp"
#include "stringoptionparser.hpp"

namespace cct {

namespace {

std::string_view ToString(const PublicExchangeName &exchangeName) { return exchangeName; }
std::string_view ToString(const PrivateExchangeName &exchangeName) { return exchangeName.str(); }

template <class ExchangeNameT>
string ConstructAccumulatedExchangeNames(std::span<const ExchangeNameT> exchangeNames) {
  string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const ExchangeNameT &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(ToString(exchangeName));
  }
  return exchangesStr;
}
}  // namespace

Coincenter::Coincenter(const PublicExchangeNames &exchangesWithoutSecrets, bool allExchangesWithoutSecrets,
                       settings::RunMode runMode, std::string_view dataDir, const MonitoringInfo &monitoringInfo)
    : _coincenterInfo(runMode, dataDir, monitoringInfo),
      _cryptowatchAPI(_coincenterInfo, runMode),
      _fiatConverter(dataDir, std::chrono::hours(8)),
      _apiKeyProvider(dataDir, exchangesWithoutSecrets, allExchangesWithoutSecrets, runMode),
      _binancePublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _bithumbPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _huobiPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _krakenPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _kucoinPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI),
      _upbitPublic(_coincenterInfo, _fiatConverter, _cryptowatchAPI) {
  for (std::string_view exchangeName : kSupportedExchanges) {
    api::ExchangePublic *exchangePublic;
    if (exchangeName == "binance") {
      exchangePublic = std::addressof(_binancePublic);
    } else if (exchangeName == "bithumb") {
      exchangePublic = std::addressof(_bithumbPublic);
    } else if (exchangeName == "huobi") {
      exchangePublic = std::addressof(_huobiPublic);
    } else if (exchangeName == "kraken") {
      exchangePublic = std::addressof(_krakenPublic);
    } else if (exchangeName == "kucoin") {
      exchangePublic = std::addressof(_kucoinPublic);
    } else if (exchangeName == "upbit") {
      exchangePublic = std::addressof(_upbitPublic);
    } else {
      throw exception("Should not happen, unsupported platform " + string(exchangeName));
    }

    const bool canUsePrivateExchange = _apiKeyProvider.contains(exchangeName);
    if (canUsePrivateExchange) {
      for (std::string_view keyName : _apiKeyProvider.getKeyNames(exchangeName)) {
        api::ExchangePrivate *exchangePrivate;
        const api::APIKey &apiKey = _apiKeyProvider.get(PrivateExchangeName(exchangeName, keyName));
        if (exchangeName == "binance") {
          exchangePrivate = std::addressof(_binancePrivates.emplace_front(_coincenterInfo, _binancePublic, apiKey));
        } else if (exchangeName == "bithumb") {
          exchangePrivate = std::addressof(_bithumbPrivates.emplace_front(_coincenterInfo, _bithumbPublic, apiKey));
        } else if (exchangeName == "huobi") {
          exchangePrivate = std::addressof(_huobiPrivates.emplace_front(_coincenterInfo, _huobiPublic, apiKey));
        } else if (exchangeName == "kraken") {
          exchangePrivate = std::addressof(_krakenPrivates.emplace_front(_coincenterInfo, _krakenPublic, apiKey));
        } else if (exchangeName == "kucoin") {
          exchangePrivate = std::addressof(_kucoinPrivates.emplace_front(_coincenterInfo, _kucoinPublic, apiKey));
        } else if (exchangeName == "upbit") {
          exchangePrivate = std::addressof(_upbitPrivates.emplace_front(_coincenterInfo, _upbitPublic, apiKey));
        } else {
          std::string err("Should not happen, unsupported platform ");
          err.append(exchangeName);
          throw exception(std::move(err));
        }

        _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic,
                                *exchangePrivate);
      }
    } else {
      _exchanges.emplace_back(_coincenterInfo.exchangeInfo(exchangePublic->name()), *exchangePublic);
    }
  }
  _exchanges.shrink_to_fit();
  _exchangeRetriever = ExchangeRetriever(_exchanges);
  _cexchangeRetriever = ConstExchangeRetriever(_exchanges);
}

void Coincenter::process(const CoincenterParsedOptions &opts) {
  if (opts.marketsCurrency != CurrencyCode()) {
    printMarkets(opts.marketsCurrency, opts.marketsExchanges);
  }

  if (opts.marketForOrderBook != Market()) {
    std::optional<int> depth;
    if (opts.orderbookDepth != 0) {
      depth = opts.orderbookDepth;
    }
    Coincenter::MarketOrderBookConversionRates marketOrderBooksConversionRates =
        getMarketOrderBooks(opts.marketForOrderBook, opts.orderBookExchanges, opts.orderbookCur, depth);
    int orderBookPos = 0;
    for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBooksConversionRates) {
      log::info("Order book of {} on {} requested{}{}", opts.marketForOrderBook.str(), exchangeName,
                optConversionRate ? " with conversion rate " : "", optConversionRate ? optConversionRate->str() : "");

      if (optConversionRate) {
        marketOrderBook.print(std::cout, exchangeName, *optConversionRate);
      } else {
        if (opts.orderbookCur != CurrencyCode::kNeutral) {
          log::warn("Unable to convert {} into {} on {}", opts.marketForOrderBook.quote().str(),
                    opts.orderbookCur.str(), exchangeName);
        }
        marketOrderBook.print(std::cout);
      }

      ++orderBookPos;
    }
  }

  if (opts.marketForConversionPath != Market()) {
    printConversionPath(opts.conversionPathExchanges, opts.marketForConversionPath);
  }

  if (opts.balanceForAll || !opts.balancePrivateExchanges.empty()) {
    printBalance(opts.balancePrivateExchanges, opts.balanceCurrencyCode);
  }

  if (!opts.startTradeAmount.isZero()) {
    MonetaryAmount startAmount = opts.startTradeAmount;
    trade(startAmount, opts.toTradeCurrency, opts.tradePrivateExchangeName, opts.tradeOptions);
  }

  if (!opts.amountToWithdraw.isZero()) {
    log::info("Withdraw gross {} from {} to {} requested", opts.amountToWithdraw.str(),
              opts.withdrawFromExchangeName.str(), opts.withdrawToExchangeName.str());
    withdraw(opts.amountToWithdraw, opts.withdrawFromExchangeName, opts.withdrawToExchangeName);
  }

  if (opts.withdrawFeeCur != CurrencyCode()) {
    printWithdrawFees(opts.withdrawFeeCur, opts.withdrawFeeExchanges);
  }

  if (opts.tradedVolumeMarket != Market()) {
    printLast24hTradedVolume(opts.tradedVolumeMarket, opts.tradedVolumeExchanges);
  }

  if (opts.lastPriceMarket != Market()) {
    printLastPrice(opts.lastPriceMarket, opts.lastPriceExchanges);
  }

  updateFileCaches();
}

Coincenter::MarketOrderBookConversionRates Coincenter::getMarketOrderBooks(
    Market m, std::span<const PublicExchangeName> exchangeNames, CurrencyCode equiCurrencyCode,
    std::optional<int> depth) {
  MarketOrderBookConversionRates ret;
  for (api::ExchangePublic *e : _exchangeRetriever.retrieveUniquePublicExchanges(exchangeNames)) {
    // Do not check if market exists when exchange names are specified to save API call
    if (!exchangeNames.empty() || e->queryTradableMarkets().contains(m)) {
      std::optional<MonetaryAmount> optConversionRate =
          equiCurrencyCode == CurrencyCode::kNeutral
              ? std::nullopt
              : e->convertAtAveragePrice(MonetaryAmount("1", m.quote()), equiCurrencyCode);
      ret.emplace_back(e->name(), MarketOrderBook(depth ? e->queryOrderBook(m, *depth) : e->queryOrderBook(m)),
                       optConversionRate);
    }
  }
  if (_coincenterInfo.useMonitoring() && !ret.empty()) {
    exportOrderbookMetrics(m, ret);
  }
  return ret;
}

BalancePortfolio Coincenter::getBalance(std::span<const PrivateExchangeName> privateExchangeNames,
                                        CurrencyCode equiCurrency) {
  bool balanceForAll = privateExchangeNames.empty();
  std::optional<CurrencyCode> optEquiCur = _coincenterInfo.fiatCurrencyIfStableCoin(equiCurrency);
  if (optEquiCur) {
    log::warn("Consider {} instead of stable coin {} as equivalent currency", optEquiCur->str(), equiCurrency.str());
    equiCurrency = *optEquiCur;
  }

  ExchangeRetriever::SelectedExchanges balanceExchanges;
  for (Exchange &exchange : _exchanges) {
    const bool computeBalance = (balanceForAll && exchange.hasPrivateAPI()) ||
                                std::any_of(privateExchangeNames.begin(), privateExchangeNames.end(),
                                            [&exchange](const PrivateExchangeName &privateExchangeName) {
                                              return exchange.matchesKeyNameWildcard(privateExchangeName);
                                            });
    if (computeBalance) {
      balanceExchanges.push_back(std::addressof(exchange));
    }
  }

  SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts> subRet(balanceExchanges.size());
  std::transform(std::execution::par, balanceExchanges.begin(), balanceExchanges.end(), subRet.begin(),
                 [equiCurrency](Exchange *e) { return e->apiPrivate().queryAccountBalance(equiCurrency); });

  if (_coincenterInfo.useMonitoring()) {
    exportBalanceMetrics(balanceExchanges, subRet, equiCurrency);
  }

  BalancePortfolio ret;
  for (const BalancePortfolio &sub : subRet) {
    ret.add(sub);
  }
  return ret;
}

void Coincenter::exportBalanceMetrics(const ExchangeRetriever::SelectedExchanges &selectedExchanges,
                                      std::span<const BalancePortfolio> balances, CurrencyCode equiCurrency) const {
  const int nbAccounts = selectedExchanges.size();
  MetricKey key("metric_name=available_balance,metric_help=Available balance in the exchange account");
  auto &metricGateway = _coincenterInfo.metricGateway();
  for (int accountPos = 0; accountPos < nbAccounts; ++accountPos) {
    const Exchange &exchange = *selectedExchanges[accountPos];
    key.set("exchange", exchange.name());
    key.set("account", exchange.keyName());
    key.set("total", "no");
    MonetaryAmount totalEquiAmount(0, equiCurrency);
    for (BalancePortfolio::MonetaryAmountWithEquivalent amountWithEqui : balances[accountPos]) {
      key.set("currency", amountWithEqui.amount.currencyCode().str());
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

void Coincenter::exportOrderbookMetrics(Market m,
                                        const MarketOrderBookConversionRates &marketOrderBookConversionRates) const {
  MetricKey key("metric_name=orderbook_pri,metric_help=Best bids and asks prices");
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
  key.set("metric_name", "orderbook_vol");
  key.set("metric_help", "Best bids and asks volumes");
  for (const auto &[exchangeName, marketOrderBook, optConversionRate] : marketOrderBookConversionRates) {
    key.set("exchange", exchangeName);
    key.set("side", "ask");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.amountAtAskPrice().toDouble());
    key.set("side", "bid");
    metricGateway.add(MetricType::kGauge, MetricOperation::kSet, key, marketOrderBook.amountAtBidPrice().toDouble());
  }
}

Coincenter::MarketsPerExchange Coincenter::getMarketsPerExchange(CurrencyCode cur,
                                                                 std::span<const PublicExchangeName> exchangeNames) {
  log::info("Query markets from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  auto uniqueExchanges = _exchangeRetriever.retrieveUniquePublicExchanges(exchangeNames);
  MarketsPerExchange marketsPerExchange(uniqueExchanges.size());
  auto marketsWithCur = [cur](api::ExchangePublic *e) {
    api::ExchangePublic::MarketSet markets = e->queryTradableMarkets();
    api::ExchangePublic::MarketSet ret;
    std::copy_if(markets.begin(), markets.end(), std::inserter(ret, ret.end()),
                 [cur](Market m) { return m.canTrade(cur); });
    return ret;
  };
  std::transform(std::execution::par, uniqueExchanges.begin(), uniqueExchanges.end(), marketsPerExchange.begin(),
                 marketsWithCur);
  return marketsPerExchange;
}

Coincenter::UniquePublicSelectedExchanges Coincenter::getExchangesTradingCurrency(
    CurrencyCode currencyCode, std::span<const PublicExchangeName> exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges =
      _exchangeRetriever.retrieveAtMostOneAccountSelectedExchanges(exchangeNames);
  using IsCurrencyTradablePerExchange = FixedCapacityVector<bool, kNbSupportedExchanges>;
  IsCurrencyTradablePerExchange isCurrencyTradablePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isCurrencyTradablePerExchange.begin(),
                 [currencyCode](Exchange *e) { return e->queryTradableCurrencies().contains(currencyCode); });

  // Erases Exchanges which do not propose asked currency (from last to first to keep index consistent)
  for (auto exchangePos = selectedExchanges.size(); exchangePos > 0; --exchangePos) {
    if (!isCurrencyTradablePerExchange[exchangePos - 1]) {
      selectedExchanges.erase(selectedExchanges.begin() + exchangePos - 1);
    }
  }
  return selectedExchanges;
}

Coincenter::UniquePublicSelectedExchanges Coincenter::getExchangesTradingMarket(
    Market m, std::span<const PublicExchangeName> exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges =
      _exchangeRetriever.retrieveAtMostOneAccountSelectedExchanges(exchangeNames);
  using IsMarketTradablePerExchange = FixedCapacityVector<bool, kNbSupportedExchanges>;
  IsMarketTradablePerExchange isMarketTradablePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 isMarketTradablePerExchange.begin(),
                 [m](Exchange *e) { return e->apiPublic().queryTradableMarkets().contains(m); });

  // Erases Exchanges which do not propose asked currency (from last to first to keep index consistent)
  for (auto exchangePos = selectedExchanges.size(); exchangePos > 0; --exchangePos) {
    if (!isMarketTradablePerExchange[exchangePos - 1]) {
      selectedExchanges.erase(selectedExchanges.begin() + exchangePos - 1);
    }
  }
  return selectedExchanges;
}

void Coincenter::printMarkets(CurrencyCode cur, std::span<const PublicExchangeName> exchangeNames) {
  log::info("Query markets from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  MarketsPerExchange marketsPerExchange = getMarketsPerExchange(cur, exchangeNames);
  string marketsCol("Markets with ");
  marketsCol.append(cur.str());
  VariadicTable<std::string_view, string> vt({"Exchange", marketsCol});
  auto exchangeIt = marketsPerExchange.begin();
  for (api::ExchangePublic *e : _exchangeRetriever.retrieveUniquePublicExchanges(exchangeNames)) {
    for (const Market &m : *exchangeIt) {
      vt.addRow(e->name(), m.str());
    }
    ++exchangeIt;
  }
  vt.print(std::cout);
}

void Coincenter::printBalance(const PrivateExchangeNames &privateExchangeNames, CurrencyCode balanceCurrencyCode) {
  log::info("Query balance from {}",
            ConstructAccumulatedExchangeNames(std::span<const PrivateExchangeName>(privateExchangeNames)));
  BalancePortfolio portfolio = getBalance(privateExchangeNames, balanceCurrencyCode);
  portfolio.print(std::cout);
}

void Coincenter::printConversionPath(std::span<const PublicExchangeName> exchangeNames, Market m) {
  log::info("Query {} conversion path from {}", m.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  VariadicTable<std::string_view, string> vt({"Exchange", "Fastest conversion path"});
  for (api::ExchangePublic *e : _exchangeRetriever.retrieveUniquePublicExchanges(exchangeNames)) {
    string conversionPathStr;
    api::ExchangePublic::ConversionPath conversionPath = e->findFastestConversionPath(m.base(), m.quote());
    if (conversionPath.empty()) {
      conversionPathStr = "--- Impossible ---";
    } else {
      for (Market m : conversionPath) {
        if (!conversionPathStr.empty()) {
          conversionPathStr.push_back(',');
        }
        conversionPathStr.append(m.assetsPairStr('-'));
      }
    }
    vt.addRow(e->name(), std::move(conversionPathStr));
  }
  vt.print();
}

MonetaryAmount Coincenter::trade(MonetaryAmount &startAmount, CurrencyCode toCurrency,
                                 const PrivateExchangeName &privateExchangeName,
                                 const api::TradeOptions &tradeOptions) {
  Exchange &exchange = _exchangeRetriever.retrieveUniqueCandidate(privateExchangeName);
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

void Coincenter::printWithdrawFees(CurrencyCode currencyCode, std::span<const PublicExchangeName> exchangeNames) {
  log::info("{} withdraw fees for {}", currencyCode.str(), ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingCurrency(currencyCode, exchangeNames);

  using WithdrawFeePerExchange = FixedCapacityVector<MonetaryAmount, kNbSupportedExchanges>;
  WithdrawFeePerExchange withdrawFeePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 withdrawFeePerExchange.begin(),
                 [currencyCode](Exchange *e) { return e->queryWithdrawalFee(currencyCode); });
  VariadicTable<std::string_view, string> vt({"Exchange", "Withdraw fee"});
  decltype(selectedExchanges)::size_type exchangePos = 0;
  for (MonetaryAmount withdrawFee : withdrawFeePerExchange) {
    vt.addRow(selectedExchanges[exchangePos++]->name(), withdrawFee.str());
  }
  vt.print();
}

Coincenter::MonetaryAmountPerExchange Coincenter::getLast24hTradedVolumePerExchange(
    Market m, std::span<const PublicExchangeName> exchangeNames) {
  log::info("Query last 24h traded volume from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(),
                 tradedVolumePerExchange.begin(), [m](Exchange *e) { return e->apiPublic().queryLast24hVolume(m); });
  return tradedVolumePerExchange;
}

Coincenter::MonetaryAmountPerExchange Coincenter::getLastPricePerExchange(
    Market m, std::span<const PublicExchangeName> exchangeNames) {
  log::info("Query last price from {}", ConstructAccumulatedExchangeNames(exchangeNames));
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange(selectedExchanges.size());
  std::transform(std::execution::par, selectedExchanges.begin(), selectedExchanges.end(), lastPricePerExchange.begin(),
                 [m](Exchange *e) { return e->apiPublic().queryLastPrice(m); });
  return lastPricePerExchange;
}

void Coincenter::printLast24hTradedVolume(Market m, std::span<const PublicExchangeName> exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange tradedVolumePerExchange = getLast24hTradedVolumePerExchange(m, exchangeNames);
  string headerTradedVolume("Last 24h ");
  headerTradedVolume.append(m.str());
  headerTradedVolume.append(" traded volume");
  VariadicTable<std::string_view, string> vt({"Exchange", headerTradedVolume});
  decltype(selectedExchanges)::size_type exchangePos = 0;
  for (MonetaryAmount tradedVolume : tradedVolumePerExchange) {
    vt.addRow(selectedExchanges[exchangePos++]->name(), tradedVolume.str());
  }
  vt.print();
}

void Coincenter::printLastPrice(Market m, std::span<const PublicExchangeName> exchangeNames) {
  UniquePublicSelectedExchanges selectedExchanges = getExchangesTradingMarket(m, exchangeNames);

  MonetaryAmountPerExchange lastPricePerExchange = getLastPricePerExchange(m, exchangeNames);
  string headerLastPrice(m.str());
  headerLastPrice.append(" last price");
  VariadicTable<std::string_view, string> vt({"Exchange", headerLastPrice});
  decltype(selectedExchanges)::size_type exchangePos = 0;
  for (MonetaryAmount lastPrice : lastPricePerExchange) {
    vt.addRow(selectedExchanges[exchangePos++]->name(), lastPrice.str());
  }
  vt.print();
}

PublicExchangeNames Coincenter::getPublicExchangeNames() const {
  std::span<const PublicExchangeName> exchangeNames;
  auto uniquePublicExchanges = _cexchangeRetriever.retrieveUniquePublicExchanges(exchangeNames);
  PublicExchangeNames ret;
  ret.reserve(uniquePublicExchanges.size());
  std::transform(std::begin(uniquePublicExchanges), std::end(uniquePublicExchanges), std::back_inserter(ret),
                 [](const api::ExchangePublic *e) { return string(e->name()); });
  return ret;
}

void Coincenter::updateFileCaches() const {
  log::debug("Store all cache files");
  _cryptowatchAPI.updateCacheFile();
  _fiatConverter.updateCacheFile();
  std::for_each(_exchanges.begin(), _exchanges.end(), [](const Exchange &e) { e.updateCacheFile(); });
}

}  // namespace cct
