#pragma once

#include <cstdint>
#include <limits>
#include <string_view>

#include "orderid.hpp"
#include "priceoptionsdef.hpp"
#include "tradeside.hpp"

namespace cct {

class TraderCommand {
 public:
  enum class Type : int8_t { kWait, kBuy, kSell, kUpdatePrice, kCancel };

  static constexpr int32_t kAllOrdersId = 0;

  /// Creates a wait command.
  static TraderCommand Wait();

  /// Creates a Place command with given intensity, side and strategy. It should be in the range [0, 100].
  static TraderCommand Place(TradeSide tradeSide, int8_t amountIntensityPercentage = 100,
                             PriceStrategy priceStrategy = PriceStrategy::maker);

  /// Creates a Cancel command with optional orderId.
  /// If orderId is not specified (or empty string), will cancel all opened orders.
  static TraderCommand Cancel(OrderIdView orderId = std::string_view());

  /// Creates an Update command for specified orderId.
  /// Equivalent to a Cancel and a Place at new price for remaining unmatched amount at the same turn.
  static TraderCommand UpdatePrice(OrderIdView orderId, PriceStrategy priceStrategy = PriceStrategy::maker);

  int32_t orderId() const { return _orderId; }

  /// If this is a Place command, return the amount intensity percentage in [0, 100]
  int8_t amountIntensityPercentage() const { return _amountIntensityPercentage; }

  Type type() const { return _type; }

  TradeSide tradeSide() const;

  PriceStrategy priceStrategy() const { return _priceStrategy; }

 private:
  static constexpr int8_t kWaitValue = 0;
  static constexpr int8_t kCancelValue = std::numeric_limits<int8_t>::min();

  TraderCommand(Type type, int32_t orderId, int8_t amountIntensityPercentage, PriceStrategy priceStrategy);

  int32_t _orderId;
  Type _type;
  int8_t _amountIntensityPercentage;
  PriceStrategy _priceStrategy;
};

}  // namespace cct