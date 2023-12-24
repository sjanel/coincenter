#pragma once

#include <cstdint>
#include <span>
#include <type_traits>

#include "cct_type_traits.hpp"
#include "closed-order.hpp"
#include "exchangeconfig.hpp"
#include "exchangeprivateapitypes.hpp"
#include "monetaryamount.hpp"
#include "opened-order.hpp"
#include "stringhelpers.hpp"
#include "trader-command.hpp"

namespace cct {

/// Contains the mutable state of the market trader engine.
class MarketTraderEngineState {
 public:
  MarketTraderEngineState(MonetaryAmount startAmountBase, MonetaryAmount startAmountQuote);

  MonetaryAmount availableBaseAmount() const { return _availableBaseAmount; }
  MonetaryAmount availableQuoteAmount() const { return _availableQuoteAmount; }

  std::span<const OpenedOrder> openedOrders() const { return _openedOrders; }
  std::span<const ClosedOrder> closedOrders() const { return _closedOrders; }

  using trivially_relocatable = std::bool_constant<is_trivially_relocatable_v<OpenedOrderVector> &&
                                                   is_trivially_relocatable_v<ClosedOrderVector>>::type;

 private:
  friend class MarketTraderEngine;

  MonetaryAmount computeBuyFrom(TraderCommand traderCommand) const;

  MonetaryAmount computeSellVolume(TraderCommand traderCommand) const;

  void placeBuyOrder(const ExchangeConfig &exchangeConfig, TimePoint placedTime, MonetaryAmount remainingVolume,
                     MonetaryAmount price, MonetaryAmount matchedVolume, MonetaryAmount from,
                     ExchangeConfig::FeeType feeType);

  void placeSellOrder(const ExchangeConfig &exchangeConfig, TimePoint placedTime, MonetaryAmount remainingVolume,
                      MonetaryAmount price, MonetaryAmount matchedVolume, ExchangeConfig::FeeType feeType);

  auto nextOrderId() { return ToString(++_nextOrderId); }

  void adjustOpenedOrderRemainingVolume(const OpenedOrder &matchedOrder, MonetaryAmount newMatchedVolume);

  void countMatchedPart(const ExchangeConfig &exchangeConfig, const OpenedOrder &matchedOrder, MonetaryAmount price,
                        MonetaryAmount newMatchedVolume, TimePoint matchedTime);

  void cancelOpenedOrder(int32_t orderId);

  OpenedOrderVector::const_iterator findOpenedOrder(int32_t orderId);

  void cancelAllOpenedOrders();

  void eraseClosedOpenedOrders(std::span<const OpenedOrder> closedOpenedOrders);

  void adjustAvailableAmountsCancel(const OpenedOrder &openedOrder);

  MonetaryAmount _availableBaseAmount;
  MonetaryAmount _availableQuoteAmount;
  OpenedOrderVector _openedOrders;
  ClosedOrderVector _closedOrders;
  int _nextOrderId{};
};
}  // namespace cct