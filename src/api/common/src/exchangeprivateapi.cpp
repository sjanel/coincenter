#include "exchangeprivateapi.hpp"

#include <chrono>
#include <thread>

#include "timedef.hpp"

namespace cct::api {

ExchangePrivate::ExchangePrivate(const CoincenterInfo &coincenterInfo, ExchangePublic &exchangePublic,
                                 const APIKey &apiKey)
    : ExchangeBase(), _exchangePublic(exchangePublic), _coincenterInfo(coincenterInfo), _apiKey(apiKey) {}

BalancePortfolio ExchangePrivate::getAccountBalance(CurrencyCode equiCurrency) {
  UniqueQueryHandle uniqueQueryHandle(_cachedResultVault);
  BalancePortfolio balancePortfolio = queryAccountBalance(equiCurrency);
  log::info("Retrieved {} balance for {} assets", _exchangePublic.name(), balancePortfolio.size());
  return balancePortfolio;
}

void ExchangePrivate::addBalance(BalancePortfolio &balancePortfolio, MonetaryAmount amount, CurrencyCode equiCurrency) {
  if (!amount.isZero()) {
    if (equiCurrency.isNeutral()) {
      log::debug("{} Balance {}", _exchangePublic.name(), amount.str());
      balancePortfolio.add(amount);
    } else {
      std::optional<MonetaryAmount> optConvertedAmountEquiCurrency = _exchangePublic.convert(amount, equiCurrency);
      MonetaryAmount equivalentInMainCurrency;
      if (optConvertedAmountEquiCurrency) {
        equivalentInMainCurrency = *optConvertedAmountEquiCurrency;
      } else {
        log::warn("Cannot convert {} into {} on {}", amount.currencyStr(), equiCurrency.str(), _exchangePublic.name());
        equivalentInMainCurrency = MonetaryAmount(0, equiCurrency);
      }
      log::debug("{} Balance {} (eq. {})", _exchangePublic.name(), amount.str(), equivalentInMainCurrency.str());
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
  log::info("{}rade {} -> {} on {}_{} requested", isMultiTradeAllowed && nbTrades > 1 ? "Multi t" : "T", from.str(),
            toCurrency.str(), _exchangePublic.name(), keyName());
  TradedAmounts tradedAmounts(from.currencyCode(), toCurrency);
  if (conversionPath.empty()) {
    log::warn("Cannot trade {} into {} on {}", from.str(), toCurrency.str(), _exchangePublic.name());
    return tradedAmounts;
  }
  if (nbTrades > 1 && !isMultiTradeAllowed) {
    log::error("Can only convert {} to {} in {} steps, but multi trade is not allowed, aborting", from.str(),
               toCurrency.str(), nbTrades);
    return tradedAmounts;
  }
  MonetaryAmount avAmount = from;
  for (int tradePos = 0; tradePos < nbTrades; ++tradePos) {
    Market m = conversionPath[tradePos];
    toCurrency = avAmount.currencyCode() == m.base() ? m.quote() : m.base();
    log::info("Step {}/{} - trade {} into {}", tradePos + 1, nbTrades, avAmount.str(), toCurrency.str());
    TradedAmounts stepTradedAmounts = marketTrade(avAmount, toCurrency, options, m);
    avAmount = stepTradedAmounts.tradedTo;
    if (avAmount.isZero()) {
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

TradedAmounts ExchangePrivate::marketTrade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options,
                                           Market m) {
  const TimePoint timerStart = Clock::now();
  const CurrencyCode fromCurrency = from.currencyCode();

  std::optional<MonetaryAmount> optPrice = _exchangePublic.computeAvgOrderPrice(m, from, options.priceOptions());
  if (!optPrice) {
    log::error("Impossible to compute {} average price on {}", _exchangePublic.name(), m.str());
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
    log::debug("Order {} closed with traded amounts {}", placeOrderInfo.orderId, placeOrderInfo.tradedAmounts().str());
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
      log::debug("Order {} closed with last traded amounts {}", placeOrderInfo.orderId, orderInfo.tradedAmounts.str());
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
      if (from.isZero()) {
        log::debug("Order {} matched with last traded amounts {} while cancelling", placeOrderInfo.orderId,
                   cancelledOrderInfo.tradedAmounts.str());
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
          log::info("Reached emergency time, make a last taker order at price {}", price.str());
        } else {
          lastPriceUpdateTime = Clock::now();
          log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
        }

        lastPrice = price;

        // Compute new volume (price is either not needed in taker order, or already recomputed)
        placeOrderInfo = placeOrderProcess(from, price, tradeInfo);

        if (placeOrderInfo.isClosed()) {
          totalTradedAmounts += placeOrderInfo.tradedAmounts();
          log::debug("Order {} closed with last traded amounts {}", placeOrderInfo.orderId,
                     placeOrderInfo.tradedAmounts().str());
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
  log::info("Withdraw {} of {} to {} initiated from {} to {}", initiatedWithdrawInfo.withdrawId(), grossAmount.str(),
            initiatedWithdrawInfo.receivingWallet().str(), _exchangePublic.name(),
            targetExchange._exchangePublic.name());
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
  log::info("Confirmed withdrawal of {} to {} {}", sentWithdrawInfo.netEmittedAmount().str(),
            initiatedWithdrawInfo.receivingWallet().exchangeName().str(),
            initiatedWithdrawInfo.receivingWallet().address());
  return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
}

namespace {
bool IsAboveDustAmountThreshold(const ExchangeInfo::MonetaryAmountSet &dustThresholds, MonetaryAmount amount) {
  auto lb = std::ranges::lower_bound(dustThresholds, amount);
  return lb == dustThresholds.end() || lb->currencyCode() != amount.currencyCode() || *lb <= amount;
}
}  // namespace

TradedAmountsVector ExchangePrivate::queryDustSweeper(CurrencyCode currencyCode) {
  using MonetaryAmountSet = ExchangeInfo::MonetaryAmountSet;
  const MonetaryAmountSet &dustThresholds = exchangeInfo().dustAmountsThreshold();
  auto dustThresholdLb = std::ranges::lower_bound(dustThresholds, MonetaryAmount(0, currencyCode));
  TradedAmountsVector ret;
  if (dustThresholdLb == dustThresholds.end() || dustThresholdLb->currencyCode() != currencyCode) {
    log::warn("No dust threshold is configured for {} on {}", currencyCode.str(), _exchangePublic.name());
    return ret;
  }
  PriceOptions priceOptions(PriceStrategy::kTaker);
  TradeOptions tradeOptions(priceOptions);
  MarketSet markets = _exchangePublic.queryTradableMarkets();
  BalancePortfolio balance = queryAccountBalance();
  MonetaryAmount dustThreshold = *dustThresholdLb;
  MonetaryAmount amountBalance;
  while ((amountBalance = balance.get(currencyCode)) >= dustThreshold) {
    // Pick a trade currency which has some available balance for which the market exists with 'currencyCode',
    // whose amount is higher than its dust amount threshold if it exists
    static constexpr int kNbTypicalPossibleMarkets = 4;
    SmallVector<Market, kNbTypicalPossibleMarkets> possibleMarkets;
    for (const BalancePortfolio::MonetaryAmountWithEquivalent &avAmountEq : balance) {
      MonetaryAmount avAmount = avAmountEq.amount;
      CurrencyCode avCur = avAmount.currencyCode();
      auto lbAvAmount = std::ranges::lower_bound(dustThresholds, MonetaryAmount(0, avCur));
      if (lbAvAmount == dustThresholds.end() || lbAvAmount->currencyCode() != avCur || *lbAvAmount < avAmount) {
        Market m(currencyCode, avCur);
        if (markets.contains(m)) {
          possibleMarkets.push_back(std::move(m));
        } else if (markets.contains(m.reverse())) {
          possibleMarkets.push_back(m.reverse());
        }
      }
    }
    SmallVector<TradedAmounts, kNbTypicalPossibleMarkets> tradedAmountsVec;
    SmallVector<MonetaryAmount, kNbTypicalPossibleMarkets> pricePerMarket;
    for (auto it = possibleMarkets.begin(); it != possibleMarkets.end(); ++it) {
      Market m = *it;
      TradedAmounts tradedAmounts = tryMarketSellOrReturnMinOrderSize(amountBalance, m);
      if (tradedAmounts.isZero()) {
        possibleMarkets.erase(it);
        --it;
        continue;
      }
      if (!tradedAmounts.tradedTo.isZero()) {
        ret.push_back(tradedAmounts);
        if (tradedAmounts.tradedFrom == amountBalance) {
          log::info("Dust {} sweeped successfully into {}", amountBalance.str(), tradedAmounts.tradedTo.str());
        }
        tradedAmountsVec.clear();
        break;
      }
      log::info("Cannot end the dust sweeper with {}, we need {}", amountBalance.str(), tradedAmounts.tradedFrom.str());
      CurrencyCode fromCur = m.base() == currencyCode ? m.quote() : m.base();
      std::optional<MonetaryAmount> optPrice = _exchangePublic.computeLimitOrderPrice(m, fromCur, priceOptions);
      if (optPrice) {
        tradedAmountsVec.push_back(tradedAmounts);
        pricePerMarket.push_back(*optPrice);
      } else {
        possibleMarkets.erase(it);
        --it;
      }
    }
    static constexpr MonetaryAmount kMultiplier(15, CurrencyCode(), 1);

    bool boughtSomething = false;
    for (MonetaryAmount mult(1); !boughtSomething; mult *= kMultiplier) {
      int pos = 0;
      for (const TradedAmounts &tradedAmounts : tradedAmountsVec) {
        // Amount of dust is too low compared to what we can sell. We need to buy additional amount so that we can
        // launch a bigger sell afterwards.
        Market m = possibleMarkets[pos];
        MonetaryAmount sanitizedVol = tradedAmounts.tradedFrom;
        MonetaryAmount from = mult * sanitizedVol;

        if (m.base() == currencyCode) {
          // We need to buy some 'base' currency.
          from *= pricePerMarket[pos];
        }

        if (balance.hasAtLeast(from) &&
            IsAboveDustAmountThreshold(dustThresholds, balance.get(from.currencyCode()) - from)) {
          TradedAmounts marketTradedAmounts = marketTrade(from, currencyCode, tradeOptions, m);
          if (marketTradedAmounts.tradedTo.isStrictlyPositive()) {
            boughtSomething = true;
            break;
          }
        }

        ++pos;
      }
    }

    balance = queryAccountBalance();
  }
  return ret;
}

PlaceOrderInfo ExchangePrivate::placeOrderProcess(MonetaryAmount &from, MonetaryAmount price,
                                                  const TradeInfo &tradeInfo) {
  Market m = tradeInfo.m;
  const bool isSell = tradeInfo.side == TradeSide::kSell;
  MonetaryAmount volume(isSell ? from : MonetaryAmount(from / price, m.base()));

  if (tradeInfo.options.isSimulation() && !isSimulatedOrderSupported()) {
    if (exchangeInfo().placeSimulateRealOrder()) {
      log::debug("Place simulate real order - price {} will be overriden", price.str());
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

  log::debug("Place new order {} at price {}", volume.str(), price.str());
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
