#include "exchangeprivateapi.hpp"

#include <chrono>
#include <thread>

#include "timehelpers.hpp"

namespace cct {
namespace api {

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
      std::optional<MonetaryAmount> optConvertedAmountEquiCurrency =
          _exchangePublic.convertAtAveragePrice(amount, equiCurrency);
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

TradedAmounts ExchangePrivate::trade(MonetaryAmount from, CurrencyCode toCurrencyCode, const TradeOptions &options) {
  log::info("{} trade {} -> {} on {}_{} requested", options.isMultiTradeAllowed() ? "Multi" : "Single", from.str(),
            toCurrencyCode.str(), _exchangePublic.name(), keyName());
  const bool realOrderPlacedInSimulationMode = !isSimulatedOrderSupported() && exchangeInfo().placeSimulateRealOrder();
  log::debug(options.str(realOrderPlacedInSimulationMode));
  TradedAmounts tradedAmounts = options.isMultiTradeAllowed()
                                    ? multiTrade(from, toCurrencyCode, options)
                                    : singleTrade(from, toCurrencyCode, options,
                                                  _exchangePublic.retrieveMarket(from.currencyCode(), toCurrencyCode));
  if (!options.isSimulation() || realOrderPlacedInSimulationMode) {
    log::info("**** Traded {} into {} ****", tradedAmounts.tradedFrom.str(), tradedAmounts.tradedTo.str());
  }
  return tradedAmounts;
}

TradedAmounts ExchangePrivate::multiTrade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options) {
  ExchangePublic::ConversionPath conversionPath =
      _exchangePublic.findFastestConversionPath(from.currencyCode(), toCurrency);
  TradedAmounts tradedAmounts(from.currencyCode(), toCurrency);
  if (conversionPath.empty()) {
    log::warn("Cannot trade {} into {} on {}", from.str(), toCurrency.str(), _exchangePublic.name());
    return tradedAmounts;
  }
  const int nbTrades = conversionPath.size();
  MonetaryAmount avAmount = from;
  for (int tradePos = 0; tradePos < nbTrades; ++tradePos) {
    Market m = conversionPath[tradePos];
    toCurrency = avAmount.currencyCode() == m.base() ? m.quote() : m.base();
    log::info("Step {}/{} - trade {} into {}", tradePos + 1, nbTrades, avAmount.str(), toCurrency.str());
    TradedAmounts stepTradedAmounts = singleTrade(avAmount, toCurrency, options, m);
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

TradedAmounts ExchangePrivate::singleTrade(MonetaryAmount from, CurrencyCode toCurrency, const TradeOptions &options,
                                           Market m) {
  const TimePoint timerStart = Clock::now();
  const CurrencyCode fromCurrency = from.currencyCode();
  TradeSide side = fromCurrency == m.base() ? TradeSide::kSell : TradeSide::kBuy;

  const auto nbSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(timerStart.time_since_epoch()).count();

  TradeInfo tradeInfo(nbSecondsSinceEpoch, m, side, options);

  MonetaryAmount price = _exchangePublic.computeAvgOrderPrice(m, from, options.priceStrategy());

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

    const bool reachedEmergencyTime = timerStart + options.maxTradeTime() < t + std::chrono::seconds(1);
    bool updatePriceNeeded = false;
    if (!reachedEmergencyTime && lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < t) {
      // Let's see if we need to change the price if limit price has changed.
      price = _exchangePublic.computeLimitOrderPrice(m, from, options.priceStrategy());
      updatePriceNeeded =
          (side == TradeSide::kSell && price < lastPrice) || (side == TradeSide::kBuy && price > lastPrice);
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
      } else {
        nextAction = NextAction::kNewOrderLimitPrice;
      }
      if (nextAction != NextAction::kWait) {
        if (nextAction == NextAction::kPlaceMarketOrder) {
          tradeInfo.options.switchToTakerStrategy();
          price = _exchangePublic.computeAvgOrderPrice(m, from, tradeInfo.options.priceStrategy());
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

WithdrawInfo ExchangePrivate::withdraw(MonetaryAmount grossAmount, ExchangePrivate &targetExchange) {
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
    std::this_thread::sleep_for(kWithdrawInfoRefreshTime);
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
            initiatedWithdrawInfo.receivingWallet().privateExchangeName().str(),
            initiatedWithdrawInfo.receivingWallet().address());
  return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
}

PlaceOrderInfo ExchangePrivate::placeOrderProcess(MonetaryAmount &from, MonetaryAmount price,
                                                  const TradeInfo &tradeInfo) {
  Market m = tradeInfo.m;
  const bool isSell = tradeInfo.side == TradeSide::kSell;
  MonetaryAmount volume(isSell ? from : MonetaryAmount(from / price, m.base()));

  if (tradeInfo.options.isSimulation() && !isSimulatedOrderSupported()) {
    if (exchangeInfo().placeSimulateRealOrder()) {
      MarketOrderBook marketOrderbook = _exchangePublic.queryOrderBook(m);
      if (isSell) {
        price = marketOrderbook.getHighestTheoreticalPrice();
      } else {
        price = marketOrderbook.getLowestTheoreticalPrice();
        volume = MonetaryAmount(from / price, m.base());
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
  PlaceOrderInfo placeOrderInfo(OrderInfo(TradedAmounts(isSell ? volume : volume.toNeutral() * price, toAmount)));
  placeOrderInfo.setClosed();
  return placeOrderInfo;
}
}  // namespace api
}  // namespace cct
