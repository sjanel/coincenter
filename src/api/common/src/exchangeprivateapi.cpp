#include "exchangeprivateapi.hpp"

#include <chrono>
#include <thread>

namespace cct {
namespace api {

void ExchangePrivate::addBalance(BalancePortfolio &balancePortfolio, MonetaryAmount amount, CurrencyCode equiCurrency) {
  if (!amount.isZero()) {
    if (equiCurrency == CurrencyCode::kNeutral) {
      log::debug("{} Balance {}", _exchangePublic.name(), amount.str());
      balancePortfolio.add(amount);
    } else {
      std::optional<MonetaryAmount> optConvertedAmountEquiCurrency =
          _exchangePublic.convertAtAveragePrice(amount, equiCurrency);
      MonetaryAmount equivalentInMainCurrency;
      if (!optConvertedAmountEquiCurrency) {
        log::warn("Cannot convert {} into {} on {}", amount.currencyCode().str(), equiCurrency.str(),
                  _exchangePublic.name());
        equivalentInMainCurrency = MonetaryAmount(0, equiCurrency);
      } else {
        equivalentInMainCurrency = *optConvertedAmountEquiCurrency;
      }
      log::debug("{} Balance {} (eq. {})", _exchangePublic.name(), amount.str(), equivalentInMainCurrency.str());
      balancePortfolio.add(amount, equivalentInMainCurrency);
    }
  }
}

MonetaryAmount ExchangePrivate::trade(MonetaryAmount &from, CurrencyCode toCurrencyCode, const TradeOptions &options) {
  MonetaryAmount initialAmount = from;
  log::info("{} trade {} -> {} on {}_{} requested", options.isMultiTradeAllowed() ? "Multi" : "Single", from.str(),
            toCurrencyCode.str(), _exchangePublic.name(), keyName());
  MonetaryAmount toAmt = options.isMultiTradeAllowed()
                             ? multiTrade(from, toCurrencyCode, options)
                             : singleTrade(from, toCurrencyCode, options,
                                           _exchangePublic.retrieveMarket(from.currencyCode(), toCurrencyCode));
  log::info("**** Traded {} into {} ****", (initialAmount - from).str(), toAmt.str());
  return toAmt;
}

MonetaryAmount ExchangePrivate::multiTrade(MonetaryAmount &from, CurrencyCode toCurrency, const TradeOptions &options) {
  MonetaryAmount initialAmount = from;
  ExchangePublic::ConversionPath conversionPath =
      _exchangePublic.findFastestConversionPath(from.currencyCode(), toCurrency);
  if (conversionPath.empty()) {
    log::error("Cannot trade {} into {} on {}", initialAmount.str(), toCurrency.str(), _exchangePublic.name());
    return MonetaryAmount(0, toCurrency, 0);
  }

  log::info(options.str());
  const int nbTrades = conversionPath.size();
  MonetaryAmount avAmount = from;
  for (int tradePos = 0; tradePos < nbTrades; ++tradePos) {
    Market m = conversionPath[tradePos];
    toCurrency = avAmount.currencyCode() == m.base() ? m.quote() : m.base();
    log::info("Step {}/{} - trade {} into {}", tradePos + 1, nbTrades, avAmount.str(), toCurrency.str());
    MonetaryAmount &stepFrom = tradePos == 0 ? from : avAmount;
    MonetaryAmount tradedTo = singleTrade(stepFrom, toCurrency, options, m);
    avAmount = tradedTo;
    if (tradedTo.isZero()) {
      break;
    }
  }
  return avAmount;
}

MonetaryAmount ExchangePrivate::singleTrade(MonetaryAmount &from, CurrencyCode toCurrency, const TradeOptions &options,
                                            Market m) {
  using Clock = TradeOptions::Clock;
  using TimePoint = TradeOptions::TimePoint;

  const TimePoint timerStart = Clock::now();
  const CurrencyCode fromCurrencyCode = from.currencyCode();

  const auto nbSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(timerStart.time_since_epoch()).count();

  TradeInfo tradeInfo(fromCurrencyCode, toCurrency, m, options, nbSecondsSinceEpoch);

  MonetaryAmount price = _exchangePublic.computeAvgOrderPrice(m, from, options.isTakerStrategy());
  MonetaryAmount volume(fromCurrencyCode == m.quote() ? MonetaryAmount(from / price, m.base()) : from);

  PlaceOrderInfo placeOrderInfo = placeOrder(from, volume, price, tradeInfo);

  // Capture by const ref is possible as we use same 'placeOrderInfo' in this method
  const OrderId &orderId = placeOrderInfo.orderId;
  if (placeOrderInfo.isClosed()) {
    log::debug("Order {} closed with traded amounts {}", orderId, placeOrderInfo.tradedAmounts().str());
    if (options.isSimulation()) {
      MonetaryAmount toAmount = fromCurrencyCode == m.quote() ? volume : volume.convertTo(price);
      toAmount = _config.exchangeInfo(_exchangePublic.name())
                     .applyFee(toAmount, options.isTakerStrategy() ? ExchangeInfo::FeeType::kTaker
                                                                   : ExchangeInfo::FeeType::kMaker);
      from -= fromCurrencyCode == m.quote() ? volume.toNeutral() * price : volume;
      return toAmount;
    }

    from -= placeOrderInfo.tradedAmounts().tradedFrom;
    return placeOrderInfo.tradedAmounts().tradedTo;
  }

  MonetaryAmount remFrom = from;
  TimePoint lastPriceUpdateTime = Clock::now();
  MonetaryAmount lastPrice = price;

  TradedAmounts totalTradedAmounts(fromCurrencyCode, toCurrency);
  do {
    OrderInfo orderInfo = queryOrderInfo(orderId, tradeInfo);
    if (orderInfo.isClosed) {
      totalTradedAmounts += orderInfo.tradedAmounts;
      log::debug("Order {} closed with last traded amounts {}", orderId, orderInfo.tradedAmounts.str());
      break;
    }

    enum class NextAction { kPlaceMarketOrder, kNewOrderLimitPrice, kWait };
    NextAction nextAction = NextAction::kWait;

    TimePoint t = Clock::now();
    const bool reachedEmergencyTime = timerStart + options.maxTradeTime() < t + options.emergencyBufferTime();
    bool updatePriceNeeded = false;
    if (!reachedEmergencyTime && lastPriceUpdateTime + options.minTimeBetweenPriceUpdates() < t) {
      // Let's see if we need to change the price if limit price has changed.
      price = _exchangePublic.computeLimitOrderPrice(m, remFrom);
      updatePriceNeeded =
          (fromCurrencyCode == m.base() && price < lastPrice) || (fromCurrencyCode == m.quote() && price > lastPrice);
    }
    if (reachedEmergencyTime || updatePriceNeeded) {
      // Cancel
      log::debug("Cancel order {}", orderId);
      OrderInfo cancelledOrderInfo = cancelOrder(orderId, tradeInfo);
      totalTradedAmounts += cancelledOrderInfo.tradedAmounts;
      remFrom -= cancelledOrderInfo.tradedAmounts.tradedFrom;
      if (remFrom.isZero()) {
        log::debug("Order {} matched with last traded amounts {} while cancelling", orderId,
                   cancelledOrderInfo.tradedAmounts.str());
        break;
      }

      if (reachedEmergencyTime) {
        // timeout. Action depends on Strategy
        if (timerStart + options.maxTradeTime() < t) {
          log::warn("Time out reached, stop from there");
          break;
        }
        if (options.strategy() == TradeStrategy::kMakerThenTaker) {
          log::info("Emergency time reached, force match as adapt strategy");
          nextAction = NextAction::kPlaceMarketOrder;
        } else {
          log::info("Emergency time reached, stop as maker strategy");
          break;
        }
      } else {
        nextAction = NextAction::kNewOrderLimitPrice;
      }
      if (nextAction != NextAction::kWait) {
        if (nextAction == NextAction::kPlaceMarketOrder) {
          tradeInfo.options.switchToTakerStrategy();
          price = _exchangePublic.computeAvgOrderPrice(m, remFrom, tradeInfo.options.isTakerStrategy());
          log::info("Reached emergency time, make a last taker order at price {}", price.str());
        } else {
          lastPriceUpdateTime = Clock::now();
          log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
        }

        lastPrice = price;

        // Compute new volume (price is either not needed in taker order, or already recomputed)
        volume = remFrom.currencyCode() == m.quote() ? MonetaryAmount(remFrom / price, m.base()) : remFrom;
        log::debug("Place new order {} at price {}", volume.str(), price.str());
        placeOrderInfo = placeOrder(remFrom, volume, price, tradeInfo);

        if (placeOrderInfo.isClosed()) {
          totalTradedAmounts += placeOrderInfo.tradedAmounts();
          log::debug("Order {} closed with last traded amounts {}", orderId, placeOrderInfo.tradedAmounts().str());
          break;
        }
      }
    }
  } while (true);

  from -= totalTradedAmounts.tradedFrom;
  return totalTradedAmounts.tradedTo;
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
}  // namespace api
}  // namespace cct
