#include "market-trader-engine-state.hpp"

#include <algorithm>
#include <cstdint>
#include <span>
#include <utility>

#include "cct_exception.hpp"
#include "closed-order.hpp"
#include "exchange-config.hpp"
#include "exchange-tradefees-config.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "stringconv.hpp"
#include "timedef.hpp"
#include "trader-command.hpp"
#include "tradeside.hpp"

namespace cct {
MarketTraderEngineState::MarketTraderEngineState(MonetaryAmount startAmountBase, MonetaryAmount startAmountQuote)
    : _availableBaseAmount(startAmountBase), _availableQuoteAmount(startAmountQuote) {}

MonetaryAmount MarketTraderEngineState::computeBuyFrom(TraderCommand traderCommand) const {
  return (_availableQuoteAmount * traderCommand.amountIntensityPercentage()) / 100;
}

MonetaryAmount MarketTraderEngineState::computeSellVolume(TraderCommand traderCommand) const {
  return (_availableBaseAmount * traderCommand.amountIntensityPercentage()) / 100;
}

void MarketTraderEngineState::placeBuyOrder(const schema::ExchangeConfig &exchangeConfig, TimePoint placedTime,
                                            MonetaryAmount remainingVolume, MonetaryAmount price,
                                            MonetaryAmount matchedVolume, MonetaryAmount from,
                                            schema::ExchangeTradeFeesConfig::FeeType feeType) {
  _availableBaseAmount += exchangeConfig.tradeFees.applyFee(matchedVolume, feeType);
  _availableQuoteAmount -= from;

  if (remainingVolume == 0) {
    _closedOrders.emplace_back(nextOrderId(), matchedVolume, price, placedTime, placedTime, TradeSide::buy);
  } else {
    _openedOrders.emplace_back(nextOrderId(), matchedVolume, remainingVolume, price, placedTime, TradeSide::buy);
  }
}

void MarketTraderEngineState::placeSellOrder(const schema::ExchangeConfig &exchangeConfig, TimePoint placedTime,
                                             MonetaryAmount remainingVolume, MonetaryAmount price,
                                             MonetaryAmount matchedVolume,
                                             schema::ExchangeTradeFeesConfig::FeeType feeType) {
  _availableBaseAmount -= (remainingVolume + matchedVolume);
  _availableQuoteAmount += exchangeConfig.tradeFees.applyFee(matchedVolume.toNeutral() * price, feeType);

  if (remainingVolume == 0) {
    _closedOrders.emplace_back(nextOrderId(), matchedVolume, price, placedTime, placedTime, TradeSide::sell);
  } else {
    _openedOrders.emplace_back(nextOrderId(), matchedVolume, remainingVolume, price, placedTime, TradeSide::sell);
  }
}

void MarketTraderEngineState::adjustOpenedOrderRemainingVolume(const OpenedOrder &matchedOrder,
                                                               MonetaryAmount newMatchedVolume) {
  auto openedOrderIt = std::ranges::find_if(
      _openedOrders, [&matchedOrder](const auto &openedOrder) { return matchedOrder.id() == openedOrder.id(); });

  *openedOrderIt = OpenedOrder(matchedOrder.id(), matchedOrder.matchedVolume() + newMatchedVolume,
                               matchedOrder.remainingVolume() - newMatchedVolume, matchedOrder.price(),
                               matchedOrder.placedTime(), matchedOrder.side());
}

void MarketTraderEngineState::countMatchedPart(const schema::ExchangeConfig &exchangeConfig,
                                               const OpenedOrder &matchedOrder, MonetaryAmount price,
                                               MonetaryAmount newMatchedVolume, TimePoint matchedTime) {
  switch (matchedOrder.side()) {
    case TradeSide::buy:
      _availableBaseAmount +=
          exchangeConfig.tradeFees.applyFee(newMatchedVolume, schema::ExchangeTradeFeesConfig::FeeType::Maker);
      break;
    case TradeSide::sell:
      _availableQuoteAmount += exchangeConfig.tradeFees.applyFee(newMatchedVolume.toNeutral() * price,
                                                                 schema::ExchangeTradeFeesConfig::FeeType::Maker);
      break;
    default:
      throw exception("Unknown trade side {}", static_cast<int>(matchedOrder.side()));
  }

  ClosedOrder newClosedOrder(matchedOrder.id(), newMatchedVolume, price, matchedOrder.placedTime(), matchedTime,
                             matchedOrder.side());

  auto closedOrderIt =
      std::ranges::find_if(_closedOrders.rbegin(), _closedOrders.rend(),
                           [&matchedOrder](const auto &closedOrder) { return closedOrder.id() == matchedOrder.id(); });
  if (closedOrderIt != _closedOrders.rend()) {
    *closedOrderIt = closedOrderIt->mergeWith(newClosedOrder);
  } else {
    _closedOrders.push_back(std::move(newClosedOrder));
  }
}

void MarketTraderEngineState::cancelOpenedOrder(int32_t orderId) {
  const auto orderIdIt = findOpenedOrder(orderId);
  adjustAvailableAmountsCancel(*orderIdIt);
  _openedOrders.erase(orderIdIt);
}

OpenedOrderVector::const_iterator MarketTraderEngineState::findOpenedOrder(int32_t orderId) {
  const auto orderIdIt = std::ranges::find_if(_openedOrders, [orderId](const OpenedOrder &openedOrder) {
    return StringToIntegral<int32_t>(openedOrder.id()) == orderId;
  });
  if (orderIdIt == _openedOrders.end()) {
    throw exception("Unable to find opened order id {}", orderId);
  }
  return orderIdIt;
}

void MarketTraderEngineState::cancelAllOpenedOrders() {
  std::ranges::for_each(_openedOrders,
                        [this](const OpenedOrder &openedOrder) { this->adjustAvailableAmountsCancel(openedOrder); });
  _openedOrders.clear();
}

void MarketTraderEngineState::adjustAvailableAmountsCancel(const OpenedOrder &openedOrder) {
  switch (openedOrder.side()) {
    case TradeSide::buy:
      _availableQuoteAmount += openedOrder.remainingVolume().toNeutral() * openedOrder.price();
      break;
    case TradeSide::sell:
      _availableBaseAmount += openedOrder.remainingVolume();
      break;
    default:
      throw exception("Unknown trade side {}", static_cast<int>(openedOrder.side()));
  }
}

void MarketTraderEngineState::eraseClosedOpenedOrders(std::span<const OpenedOrder> closedOpenedOrders) {
  const auto [first, last] = std::ranges::remove_if(_openedOrders, [closedOpenedOrders](const auto &openedOrder) {
    return std::ranges::any_of(closedOpenedOrders, [&openedOrder](const auto &closedOpenedOrder) {
      return openedOrder.id() == closedOpenedOrder.id();
    });
  });
  _openedOrders.erase(first, last);
}

}  // namespace cct