#include "coincentercommand.hpp"

#include "cct_exception.hpp"

namespace cct {
bool CoincenterCommand::isPublic() const {
  switch (_type) {
    case Type::kMarkets:
      [[fallthrough]];
    case Type::kConversionPath:
      [[fallthrough]];
    case Type::kLastPrice:
      [[fallthrough]];
    case Type::kTicker:
      [[fallthrough]];
    case Type::kOrderbook:
      [[fallthrough]];
    case Type::kLastTrades:
      [[fallthrough]];
    case Type::kLast24hTradedVolume:
      [[fallthrough]];
    case Type::kWithdrawFee:
      return true;
    default:
      return false;
  }
}

bool CoincenterCommand::isReadOnly() const {
  if (isPublic()) {
    return true;
  }
  switch (_type) {
    case Type::kBalance:
      [[fallthrough]];
    case Type::kDepositInfo:
      [[fallthrough]];
    case Type::kOrdersOpened:
      return true;
    default:
      return false;
  }
}

CoincenterCommand& CoincenterCommand::setExchangeNames(const ExchangeNames& exchangeNames) {
  _exchangeNames = exchangeNames;
  return *this;
}
CoincenterCommand& CoincenterCommand::setExchangeNames(ExchangeNames&& exchangeNames) {
  _exchangeNames = std::move(exchangeNames);
  return *this;
}

CoincenterCommand& CoincenterCommand::setOrdersConstraints(const OrdersConstraints& ordersConstraints) {
  if (_type != Type::kOrdersCancel && _type != Type::kOrdersOpened) {
    throw exception("Order constraints can only be used for orders related commands");
  }
  _ordersConstraints = ordersConstraints;
  return *this;
}

CoincenterCommand& CoincenterCommand::setOrdersConstraints(OrdersConstraints&& ordersConstraints) {
  if (_type != Type::kOrdersCancel && _type != Type::kOrdersOpened) {
    throw exception("Order constraints can only be used for orders related commands");
  }
  _ordersConstraints = std::move(ordersConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setAmount(MonetaryAmount amount) {
  _amount = amount;
  return *this;
}

CoincenterCommand& CoincenterCommand::setRepeats(int repeats) {
  if (isWrite()) {
    throw exception("Write commands cannot be repeated");
  }
  _repeats = repeats;
  return *this;
}

CoincenterCommand& CoincenterCommand::setN(int n) {
  _n = n;
  return *this;
}

CoincenterCommand& CoincenterCommand::setMarket(Market market) {
  _market = market;
  return *this;
}

CoincenterCommand& CoincenterCommand::setCur1(CurrencyCode cur1) {
  _cur1 = cur1;
  return *this;
}

CoincenterCommand& CoincenterCommand::setCur2(CurrencyCode cur2) {
  if (_cur1.isNeutral()) {
    throw exception("First currency should be set before second one");
  }
  _cur2 = cur2;
  return *this;
}

CoincenterCommand& CoincenterCommand::setPercentageTrade() {
  if (_type != Type::kBuy && _type != Type::kSell && _type != Type::kTrade) {
    throw exception("Percentage trade can only be set for trade / buy / sell command");
  }
  _isPercentageTrade = true;
  return *this;
}
}  // namespace cct