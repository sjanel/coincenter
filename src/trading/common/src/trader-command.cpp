#include "trader-command.hpp"

#include <cstdint>

#include "cct_exception.hpp"
#include "orderid.hpp"
#include "priceoptionsdef.hpp"
#include "stringconv.hpp"
#include "tradeside.hpp"

namespace cct {
TraderCommand::TraderCommand(Type type, int32_t orderId, int8_t amountIntensityPercentage, PriceStrategy priceStrategy)
    : _orderId(orderId),
      _type(type),
      _amountIntensityPercentage(amountIntensityPercentage),
      _priceStrategy(priceStrategy) {}

TraderCommand TraderCommand::Wait() { return {Type::kWait, kAllOrdersId, 0, PriceStrategy::maker}; }

TraderCommand TraderCommand::Place(TradeSide tradeSide, int8_t amountIntensityPercentage, PriceStrategy priceStrategy) {
  if (amountIntensityPercentage > 100 || amountIntensityPercentage <= 0) {
    throw exception("Invalid amountIntensityPercentage {}", amountIntensityPercentage);
  }
  Type type;
  switch (tradeSide) {
    case TradeSide::buy:
      type = Type::kBuy;
      break;
    case TradeSide::sell:
      type = Type::kSell;
      break;
    default:
      throw exception("Unexpected trade side");
  }
  return {type, kAllOrdersId, amountIntensityPercentage, priceStrategy};
}

TraderCommand TraderCommand::Cancel(OrderIdView orderId) {
  int32_t orderIdInt;
  if (!orderId.empty()) {
    orderIdInt = StringToIntegral<int32_t>(orderId);
  } else {
    orderIdInt = kAllOrdersId;
  }
  return {Type::kCancel, orderIdInt, 0, PriceStrategy::maker};
}

TraderCommand TraderCommand::UpdatePrice(OrderIdView orderId, PriceStrategy priceStrategy) {
  return {Type::kUpdatePrice, StringToIntegral<int32_t>(orderId), 100, priceStrategy};
}

TradeSide TraderCommand::tradeSide() const {
  switch (_type) {
    case Type::kBuy:
      return TradeSide::buy;
    case Type::kSell:
      return TradeSide::sell;
    default:
      throw exception("Unexpected trade command type for trade side");
  }
}

}  // namespace cct