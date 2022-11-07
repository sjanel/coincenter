#include "exchangeprivateapi.hpp"

#include <chrono>
#include <map>
#include <thread>

#include "cct_vector.hpp"
#include "coincenterinfo.hpp"
#include "timedef.hpp"

namespace cct::api {

ExchangePrivate::ExchangePrivate(const CoincenterInfo &coincenterInfo, ExchangePublic &exchangePublic,
                                 const APIKey &apiKey)
    : ExchangeBase(), _exchangePublic(exchangePublic), _coincenterInfo(coincenterInfo), _apiKey(apiKey) {}

BalancePortfolio ExchangePrivate::getAccountBalance(const BalanceOptions &balanceOptions) {
  UniqueQueryHandle uniqueQueryHandle(_cachedResultVault);
  BalancePortfolio balancePortfolio = queryAccountBalance(balanceOptions);
  log::info("Retrieved {} balance for {} assets", _exchangePublic.name(), balancePortfolio.size());
  return balancePortfolio;
}

void ExchangePrivate::addBalance(BalancePortfolio &balancePortfolio, MonetaryAmount amount, CurrencyCode equiCurrency) {
  if (amount != 0) {
    if (equiCurrency.isNeutral()) {
      log::debug("{} Balance {}", _exchangePublic.name(), amount);
      balancePortfolio.add(amount);
    } else {
      std::optional<MonetaryAmount> optConvertedAmountEquiCurrency = _exchangePublic.convert(amount, equiCurrency);
      MonetaryAmount equivalentInMainCurrency;
      if (optConvertedAmountEquiCurrency) {
        equivalentInMainCurrency = *optConvertedAmountEquiCurrency;
      } else {
        log::warn("Cannot convert {} into {} on {}", amount.currencyStr(), equiCurrency, _exchangePublic.name());
        equivalentInMainCurrency = MonetaryAmount(0, equiCurrency);
      }
      log::debug("{} Balance {} (eq. {})", _exchangePublic.name(), amount, equivalentInMainCurrency);
      balancePortfolio.add(amount, equivalentInMainCurrency);
    }
  }
}

TradedAmounts ExchangePrivate::trade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options,
                                     const MarketsPath &conversionPath) {
  const bool realOrderPlacedInSimulationMode = !isSimulatedOrderSupported() && exchangeInfo().placeSimulateRealOrder();
  log::debug(options.str(realOrderPlacedInSimulationMode));
  const int nbTrades = static_cast<int>(conversionPath.size());
  const bool isMultiTradeAllowed = options.isMultiTradeAllowed(exchangeInfo().multiTradeAllowedByDefault());
  log::info("{}rade {} -> {} on {}_{} requested", isMultiTradeAllowed && nbTrades > 1 ? "Multi t" : "T", from,
            toCurrency, _exchangePublic.name(), keyName());
  TradedAmounts tradedAmounts(from.currencyCode(), toCurrency);
  if (conversionPath.empty()) {
    log::warn("Cannot trade {} into {} on {}", from, toCurrency, _exchangePublic.name());
    return tradedAmounts;
  }
  if (nbTrades > 1 && !isMultiTradeAllowed) {
    log::error("Can only convert {} to {} in {} steps, but multi trade is not allowed, aborting", from, toCurrency,
               nbTrades);
    return tradedAmounts;
  }
  MonetaryAmount avAmount = from;
  for (int tradePos = 0; tradePos < nbTrades; ++tradePos) {
    Market m = conversionPath[tradePos];
    log::info("Step {}/{} - trade {} into {}", tradePos + 1, nbTrades, avAmount, m.opposite(avAmount.currencyCode()));
    TradedAmounts stepTradedAmounts = marketTrade(avAmount, options, m);
    avAmount = stepTradedAmounts.tradedTo;
    if (avAmount == 0) {
      break;
    }
    if (tradePos == 0) {
      tradedAmounts.tradedFrom = stepTradedAmounts.tradedFrom;
    }
    if (tradePos + 1 == nbTrades) {
      tradedAmounts.tradedTo = stepTradedAmounts.tradedTo;
    }
  }
  return tradedAmounts;
}

TradedAmounts ExchangePrivate::marketTrade(MonetaryAmount from, const TradeOptions &options, Market m) {
  const TimePoint timerStart = Clock::now();
  const CurrencyCode fromCurrency = from.currencyCode();

  std::optional<MonetaryAmount> optPrice = _exchangePublic.computeAvgOrderPrice(m, from, options.priceOptions());
  const CurrencyCode toCurrency = m.opposite(fromCurrency);
  if (!optPrice) {
    log::error("Impossible to compute {} average price on {}", _exchangePublic.name(), m);
    return TradedAmounts(fromCurrency, toCurrency);
  }

  MonetaryAmount price = *optPrice;
  const auto nbSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(timerStart.time_since_epoch()).count();
  const bool noEmergencyTime = options.maxTradeTime() == Duration::max();

  TradeSide side = fromCurrency == m.base() ? TradeSide::kSell : TradeSide::kBuy;
  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, side, options);
  PlaceOrderInfo placeOrderInfo = placeOrderProcess(from, price, tradeInfo);

  if (placeOrderInfo.isClosed()) {
    log::debug("Order {} closed with traded amounts {}", placeOrderInfo.orderId, placeOrderInfo.tradedAmounts());
    return placeOrderInfo.tradedAmounts();
  }

  OrderRef orderRef(placeOrderInfo.orderId, nbSecondsSinceEpoch, m, side);

  TimePoint lastPriceUpdateTime = Clock::now();
  MonetaryAmount lastPrice = price;

  TradedAmounts totalTradedAmounts(fromCurrency, toCurrency);
  do {
    OrderInfo orderInfo = queryOrderInfo(orderRef);
    if (orderInfo.isClosed) {
      totalTradedAmounts += orderInfo.tradedAmounts;
      log::debug("Order {} closed with last traded amounts {}", placeOrderInfo.orderId, orderInfo.tradedAmounts);
      break;
    }

    enum class NextAction { kPlaceMarketOrder, kNewOrderLimitPrice, kWait };
    NextAction nextAction = NextAction::kWait;

    TimePoint t = Clock::now();

    const bool reachedEmergencyTime =
        !noEmergencyTime && timerStart + options.maxTradeTime() < t + std::chrono::seconds(1);
    bool updatePriceNeeded = false;
    if (!options.isFixedPrice() && !reachedEmergencyTime &&
        lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < t) {
      // Let's see if we need to change the price if limit price has changed.
      optPrice = _exchangePublic.computeLimitOrderPrice(m, fromCurrency, options.priceOptions());
      if (optPrice) {
        price = *optPrice;
        updatePriceNeeded =
            (side == TradeSide::kSell && price < lastPrice) || (side == TradeSide::kBuy && price > lastPrice);
      }
    }
    if (reachedEmergencyTime || updatePriceNeeded) {
      // Cancel
      log::debug("Cancel order {}", placeOrderInfo.orderId);
      OrderInfo cancelledOrderInfo = cancelOrder(orderRef);
      totalTradedAmounts += cancelledOrderInfo.tradedAmounts;
      from -= cancelledOrderInfo.tradedAmounts.tradedFrom;
      if (from == 0) {
        log::debug("Order {} matched with last traded amounts {} while cancelling", placeOrderInfo.orderId,
                   cancelledOrderInfo.tradedAmounts);
        break;
      }

      if (reachedEmergencyTime) {
        // timeout. Action depends on Strategy
        if (timerStart + options.maxTradeTime() < t) {
          log::warn("Time out reached, stop from there");
          break;
        }
        log::info("Emergency time reached, {} trade", options.timeoutActionStr());
        if (options.placeMarketOrderAtTimeout()) {
          nextAction = NextAction::kPlaceMarketOrder;
        } else {
          break;
        }
      } else {  // updatePriceNeeded
        nextAction = NextAction::kNewOrderLimitPrice;
      }
      if (nextAction != NextAction::kWait) {
        if (nextAction == NextAction::kPlaceMarketOrder) {
          tradeInfo.options.switchToTakerStrategy();
          optPrice = _exchangePublic.computeAvgOrderPrice(m, from, tradeInfo.options.priceOptions());
          if (!optPrice) {
            throw exception("Impossible to compute new average order price");
          }
          price = *optPrice;
          log::info("Reached emergency time, make a last taker order at price {}", price);
        } else {
          lastPriceUpdateTime = Clock::now();
          log::info("Limit price changed from {} to {}, update order", lastPrice, price);
        }

        lastPrice = price;

        // Compute new volume (price is either not needed in taker order, or already recomputed)
        placeOrderInfo = placeOrderProcess(from, price, tradeInfo);

        if (placeOrderInfo.isClosed()) {
          totalTradedAmounts += placeOrderInfo.tradedAmounts();
          log::debug("Order {} closed with last traded amounts {}", placeOrderInfo.orderId,
                     placeOrderInfo.tradedAmounts());
          break;
        }
      }
    }
  } while (true);

  return totalTradedAmounts;
}

WithdrawInfo ExchangePrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange,
                                       Duration withdrawRefreshTime) {
  const CurrencyCode currencyCode = grossAmount.currencyCode();
  InitiatedWithdrawInfo initiatedWithdrawInfo =
      launchWithdraw(grossAmount, targetExchange.queryDepositWallet(currencyCode));
  log::info("Withdraw {} of {} to {} initiated from {} to {}", initiatedWithdrawInfo.withdrawId(), grossAmount,
            initiatedWithdrawInfo.receivingWallet(), _exchangePublic.name(), targetExchange._exchangePublic.name());
  enum class NextAction { kCheckSender, kCheckReceiver, kTerminate };
  NextAction action = NextAction::kCheckSender;
  SentWithdrawInfo sentWithdrawInfo;
  do {
    std::this_thread::sleep_for(withdrawRefreshTime);
    switch (action) {
      case NextAction::kCheckSender:
        sentWithdrawInfo = isWithdrawSuccessfullySent(initiatedWithdrawInfo);
        if (sentWithdrawInfo.isWithdrawSent()) {
          log::info("Withdraw successfully sent from {}", _exchangePublic.name());
          action = NextAction::kCheckReceiver;
        }
        break;
      case NextAction::kCheckReceiver:
        if (targetExchange.isWithdrawReceived(initiatedWithdrawInfo, sentWithdrawInfo)) {
          log::info("Withdraw successfully received at {}", targetExchange._exchangePublic.name());
          action = NextAction::kTerminate;
        }
        break;
      case NextAction::kTerminate:
        break;
    }
  } while (action != NextAction::kTerminate);
  log::info("Confirmed withdrawal of {} to {} {}", sentWithdrawInfo.netEmittedAmount(),
            initiatedWithdrawInfo.receivingWallet().exchangeName(), initiatedWithdrawInfo.receivingWallet().address());
  return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
}

namespace {
bool IsAboveDustAmountThreshold(const MonetaryAmountByCurrencySet &dustThresholds, MonetaryAmount amount) {
  auto foundIt = dustThresholds.find(amount);
  return foundIt == dustThresholds.end() || *foundIt <= amount;
}

using PenaltyPerMarketMap = std::map<Market, int>;

void IncrementPenalty(Market m, PenaltyPerMarketMap &penaltyPerMarketMap) {
  auto insertItInsertedPair = penaltyPerMarketMap.insert({m, 1});
  if (!insertItInsertedPair.second) {
    ++insertItInsertedPair.first->second;
  }
}

vector<Market> GetPossibleMarketsForDustThresholds(const BalancePortfolio &balance,
                                                   const MonetaryAmountByCurrencySet &dustThresholds,
                                                   CurrencyCode currencyCode, const MarketSet &markets,
                                                   const PenaltyPerMarketMap &penaltyPerMarketMap) {
  vector<Market> possibleMarkets;
  for (const BalancePortfolio::MonetaryAmountWithEquivalent &avAmountEq : balance) {
    MonetaryAmount avAmount = avAmountEq.amount;
    CurrencyCode avCur = avAmount.currencyCode();
    auto lbAvAmount = dustThresholds.find(MonetaryAmount(0, avCur));
    if (lbAvAmount == dustThresholds.end() || *lbAvAmount < avAmount) {
      Market m(currencyCode, avCur);
      if (markets.contains(m)) {
        possibleMarkets.push_back(std::move(m));
      } else if (markets.contains(m.reverse())) {
        possibleMarkets.push_back(m.reverse());
      }
    }
  }

  struct PenaltyMarketComparator {
    explicit PenaltyMarketComparator(const PenaltyPerMarketMap &map) : penaltyPerMarketMap(map) {}

    bool operator()(Market m1, Market m2) const {
      auto m1It = penaltyPerMarketMap.find(m1);
      auto m2It = penaltyPerMarketMap.find(m2);
      // not present is equivalent to a weight of 0
      int w1 = m1It == penaltyPerMarketMap.end() ? 0 : m1It->second;
      int w2 = m2It == penaltyPerMarketMap.end() ? 0 : m2It->second;

      if (w1 != w2) {
        return w1 < w2;
      }
      return m1 < m2;
    }

    const PenaltyPerMarketMap &penaltyPerMarketMap;
  };

  // Sort them according to the penalty (we favor markets on which we did not try any buy on them yet)
  std::ranges::sort(possibleMarkets, PenaltyMarketComparator(penaltyPerMarketMap));
  return possibleMarkets;
}
}  // namespace

std::pair<TradedAmounts, Market> ExchangePrivate::isSellingPossibleOneShotDustSweeper(
    std::span<const Market> possibleMarkets, MonetaryAmount amountBalance, const TradeOptions &tradeOptions) {
  for (Market m : possibleMarkets) {
    log::info("Dust sweeper - attempt to sell in one shot on {}", m);
    TradedAmounts tradedAmounts = marketTrade(amountBalance, tradeOptions, m);
    if (tradedAmounts.tradedTo != 0) {
      return {tradedAmounts, m};
    }
  }
  return {};
}

TradedAmounts ExchangePrivate::buySomeAmountToMakeFutureSellPossible(
    std::span<const Market> possibleMarkets, MarketPriceMap &marketPriceMap, MonetaryAmount dustThreshold,
    const BalancePortfolio &balance, const TradeOptions &tradeOptions,
    const MonetaryAmountByCurrencySet &dustThresholds) {
  CurrencyCode currencyCode = dustThreshold.currencyCode();
  static constexpr MonetaryAmount kMultiplier(15, CurrencyCode(), 1);

  if (marketPriceMap.empty()) {
    marketPriceMap = _exchangePublic.queryAllPrices();
  }

  for (MonetaryAmount mult = MonetaryAmount(1);; mult *= kMultiplier) {
    bool enoughAvAmount = false;
    for (Market m : possibleMarkets) {
      // We will buy some amount. It should be as small as possible to limit fees
      auto it = marketPriceMap.find(m);
      if (it == marketPriceMap.end()) {
        continue;
      }

      // Compute initial fromAmount from a random small amount defined from current price and dust threshold
      // (assuming it's rather small)
      MonetaryAmount p = it->second;
      MonetaryAmount fromAmount;
      if (currencyCode == m.base()) {
        fromAmount = MonetaryAmount(dustThreshold * p.toNeutral(), m.quote());
      } else {
        fromAmount = MonetaryAmount(dustThreshold / p.toNeutral(), m.base());
      }

      fromAmount *= mult;

      MonetaryAmount fromAmountAv = balance.get(fromAmount.currencyCode());
      if (fromAmountAv < fromAmount || !IsAboveDustAmountThreshold(dustThresholds, fromAmountAv - fromAmount)) {
        // The resulting sell of fromAmount should not bring this other currency below the dust thresholds,
        // it's counter productive
        continue;
      }

      enoughAvAmount = true;

      log::info("Dust sweeper - attempt to buy some {} for future selling", currencyCode);
      TradedAmounts tradedAmounts = marketTrade(fromAmount, tradeOptions, m);

      if (tradedAmounts.tradedTo != 0) {
        // Then we should have sufficient amount now on this market
        return tradedAmounts;
      }
    }
    if (!enoughAvAmount) {
      break;
    }
  }
  return TradedAmounts{};
}

TradedAmountsVectorWithFinalAmount ExchangePrivate::queryDustSweeper(CurrencyCode currencyCode) {
  const MonetaryAmountByCurrencySet &dustThresholds = exchangeInfo().dustAmountsThreshold();
  const int dustSweeperMaxNbTrades = exchangeInfo().dustSweeperMaxNbTrades();
  auto dustThresholdLb = dustThresholds.find(MonetaryAmount(0, currencyCode));
  TradedAmountsVectorWithFinalAmount ret;
  auto eName = exchangeName();
  if (dustThresholdLb == dustThresholds.end()) {
    log::warn("No dust threshold is configured for {} on {:n}", currencyCode, eName);
    return ret;
  }
  const MonetaryAmount dustThreshold = *dustThresholdLb;

  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  MarketSet markets = _exchangePublic.queryTradableMarkets();
  MarketPriceMap marketPriceMap;
  bool checkAmountBalanceAgainstDustThreshold = true;
  int dustSweeperTradePos;
  PenaltyPerMarketMap penaltyPerMarketMap;
  Market tradedMarket;
  for (dustSweeperTradePos = 0; dustSweeperTradePos < dustSweeperMaxNbTrades; ++dustSweeperTradePos) {
    BalancePortfolio balance = queryAccountBalance();
    ret.finalAmount = balance.get(currencyCode);
    log::info("Dust sweeper for {} - step {}/{} - {} remaining", eName, dustSweeperTradePos + 1, dustSweeperMaxNbTrades,
              ret.finalAmount);
    if (ret.finalAmount == 0) {
      if (checkAmountBalanceAgainstDustThreshold) {
        log::info("Already no {} present in {} balance", currencyCode, eName);
      } else {
        log::info("Successfully sold all {} on {}", currencyCode, eName);
      }
      return ret;
    }
    if (checkAmountBalanceAgainstDustThreshold && dustThreshold < ret.finalAmount) {
      log::warn("Initial amount balance {} is larger that dust threshold {} on {}, abort", ret.finalAmount,
                dustThreshold, eName);
      return ret;
    }
    checkAmountBalanceAgainstDustThreshold = false;

    // Pick a trade currency which has some available balance for which the market exists with 'currencyCode',
    // whose amount is higher than its dust amount threshold if it exists
    vector<Market> possibleMarkets =
        GetPossibleMarketsForDustThresholds(balance, dustThresholds, currencyCode, markets, penaltyPerMarketMap);
    if (possibleMarkets.empty()) {
      log::warn("No more market is allowed for trade in dust threshold sweeper context");
      break;
    }

    // First pass - check if by chance on selected markets selling is possible in one shot
    TradedAmounts tradedAmounts;
    std::tie(tradedAmounts, tradedMarket) =
        isSellingPossibleOneShotDustSweeper(possibleMarkets, ret.finalAmount, tradeOptions);
    if (tradedAmounts.tradedFrom != 0) {
      IncrementPenalty(tradedMarket, penaltyPerMarketMap);
      ret.tradedAmountsVector.push_back(std::move(tradedAmounts));
      continue;
    }

    // At this point we did not sell all amount, but it's possible that some trades have been done, with remainings.
    // Selling has not worked - so we need to buy some amount on the requested currency first
    tradedAmounts = buySomeAmountToMakeFutureSellPossible(possibleMarkets, marketPriceMap, dustThreshold, balance,
                                                          tradeOptions, dustThresholds);
    if (tradedAmounts.tradedFrom == 0) {
      break;
    }
    ret.tradedAmountsVector.push_back(std::move(tradedAmounts));
  }
  log::warn("Could not sell dust on {} after {} tries", eName, dustSweeperTradePos + 1);
  return ret;
}

PlaceOrderInfo ExchangePrivate::placeOrderProcess(MonetaryAmount &from, MonetaryAmount price,
                                                  const TradeInfo &tradeInfo) {
  Market m = tradeInfo.m;
  const bool isSell = tradeInfo.side == TradeSide::kSell;
  MonetaryAmount volume(isSell ? from : MonetaryAmount(from / price, m.base()));

  if (tradeInfo.options.isSimulation() && !isSimulatedOrderSupported()) {
    if (exchangeInfo().placeSimulateRealOrder()) {
      log::debug("Place simulate real order - price {} will be overriden", price);
      MarketOrderBook marketOrderbook = _exchangePublic.queryOrderBook(m);
      if (isSell) {
        price = marketOrderbook.getHighestTheoreticalPrice();
      } else {
        price = marketOrderbook.getLowestTheoreticalPrice();
      }
    } else {
      PlaceOrderInfo placeOrderInfo = computeSimulatedMatchedPlacedOrderInfo(volume, price, tradeInfo);
      from -= placeOrderInfo.tradedAmounts().tradedFrom;
      return placeOrderInfo;
    }
  }

  log::debug("Place new order {} at price {}", volume, price);
  PlaceOrderInfo placeOrderInfo = placeOrder(from, volume, price, tradeInfo);
  if (tradeInfo.options.isSimulation() && isSimulatedOrderSupported()) {
    // Override the placeOrderInfo in simulation mode to centralize code which is same for all exchanges
    // (and remove the need to implement the matching amount computation with fees for each exchange)
    placeOrderInfo = computeSimulatedMatchedPlacedOrderInfo(volume, price, tradeInfo);
  }
  from -= placeOrderInfo.tradedAmounts().tradedFrom;
  return placeOrderInfo;
}

PlaceOrderInfo ExchangePrivate::computeSimulatedMatchedPlacedOrderInfo(MonetaryAmount volume, MonetaryAmount price,
                                                                       const TradeInfo &tradeInfo) const {
  const bool placeSimulatedRealOrder = exchangeInfo().placeSimulateRealOrder();
  const bool isTakerStrategy = tradeInfo.options.isTakerStrategy(placeSimulatedRealOrder);
  const bool isSell = tradeInfo.side == TradeSide::kSell;
  MonetaryAmount toAmount = isSell ? volume.convertTo(price) : volume;
  ExchangeInfo::FeeType feeType = isTakerStrategy ? ExchangeInfo::FeeType::kTaker : ExchangeInfo::FeeType::kMaker;
  toAmount = _coincenterInfo.exchangeInfo(_exchangePublic.name()).applyFee(toAmount, feeType);
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(isSell ? volume : volume.toNeutral() * price, toAmount)),
                                OrderId("SimulatedOrderId"));
  placeOrderInfo.setClosed();
  return placeOrderInfo;
}
}  // namespace cct::api
