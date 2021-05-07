#include "exchangeprivateapi.hpp"

#include <chrono>
#include <thread>

namespace cct {
namespace api {

MonetaryAmount ExchangePrivate::trade(MonetaryAmount &from, CurrencyCode toCurrencyCode, const TradeOptions &options) {
  using Clock = TradeOptions::Clock;
  using TimePoint = TradeOptions::TimePoint;

  const TimePoint timerStart = Clock::now();
  const CurrencyCode fromCurrencyCode = from.currencyCode();
  const Market m = _exchangePublic.retrieveMarket(fromCurrencyCode, toCurrencyCode);

  const auto nbSecondsSinceEpoch =
      std::chrono::duration_cast<std::chrono::seconds>(timerStart.time_since_epoch()).count();

  TradeInfo tradeInfo(fromCurrencyCode, toCurrencyCode, m, options,
                      std::to_string(static_cast<int32_t>(nbSecondsSinceEpoch)));

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

  TradedAmounts totalTradedAmounts(fromCurrencyCode, toCurrencyCode);
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
        // Compute new volume (price is either not needed in taker order, or already recomputed)
        volume = remFrom.currencyCode() == m.quote() ? MonetaryAmount(remFrom / price, m.base()) : remFrom;
        if (nextAction == NextAction::kPlaceMarketOrder) {
          log::warn("Reaching emergency time, make a last order at market price");
          tradeInfo.options.switchToTakerStrategy();
          price = _exchangePublic.computeAvgOrderPrice(m, remFrom, true /* isTakerStrategy */);
        } else {
          lastPriceUpdateTime = Clock::now();
          log::info("Limit price changed from {} to {}, update order", lastPrice.str(), price.str());
        }

        lastPrice = price;

        // Place new order with 'volume' and 'price' updated
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
  log::warn("Confirmed withdrawal of {} to {} {}", sentWithdrawInfo.netEmittedAmount().str(),
            initiatedWithdrawInfo.receivingWallet().privateExchangeName().str(),
            initiatedWithdrawInfo.receivingWallet().address());
  return WithdrawInfo(initiatedWithdrawInfo, sentWithdrawInfo);
}
}  // namespace api
}  // namespace cct
