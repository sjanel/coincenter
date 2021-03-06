#include "coincentercommand.hpp"

#include "cct_exception.hpp"

namespace cct {
bool CoincenterCommand::isPublic() const {
  switch (_type) {
    case CoincenterCommandType::kMarkets:
      [[fallthrough]];
    case CoincenterCommandType::kConversionPath:
      [[fallthrough]];
    case CoincenterCommandType::kLastPrice:
      [[fallthrough]];
    case CoincenterCommandType::kTicker:
      [[fallthrough]];
    case CoincenterCommandType::kOrderbook:
      [[fallthrough]];
    case CoincenterCommandType::kLastTrades:
      [[fallthrough]];
    case CoincenterCommandType::kLast24hTradedVolume:
      [[fallthrough]];
    case CoincenterCommandType::kWithdrawFee:
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
    case CoincenterCommandType::kBalance:
      [[fallthrough]];
    case CoincenterCommandType::kDepositInfo:
      [[fallthrough]];
    case CoincenterCommandType::kOrdersOpened:
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
  if (_type != CoincenterCommandType::kOrdersCancel && _type != CoincenterCommandType::kOrdersOpened) {
    throw exception("Order constraints can only be used for orders related commands");
  }
  _tradeOrOrdersOptions = ordersConstraints;
  return *this;
}

CoincenterCommand& CoincenterCommand::setOrdersConstraints(OrdersConstraints&& ordersConstraints) {
  if (_type != CoincenterCommandType::kOrdersCancel && _type != CoincenterCommandType::kOrdersOpened) {
    throw exception("Order constraints can only be used for orders related commands");
  }
  _tradeOrOrdersOptions = std::move(ordersConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setTradeOptions(const TradeOptions& tradeOptions) {
  if (_type != CoincenterCommandType::kBuy && _type != CoincenterCommandType::kSell &&
      _type != CoincenterCommandType::kTrade) {
    throw exception("Trade options can only be used for trade related commands");
  }
  _tradeOrOrdersOptions = tradeOptions;
  return *this;
}

CoincenterCommand& CoincenterCommand::setTradeOptions(TradeOptions&& tradeOptions) {
  if (_type != CoincenterCommandType::kBuy && _type != CoincenterCommandType::kSell &&
      _type != CoincenterCommandType::kTrade) {
    throw exception("Trade options can only be used for trade related commands");
  }
  _tradeOrOrdersOptions = std::move(tradeOptions);
  return *this;
}

CoincenterCommand& CoincenterCommand::setAmount(MonetaryAmount amount) {
  _amount = amount;
  return *this;
}

CoincenterCommand& CoincenterCommand::setDepth(int d) {
  _n = d;
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

CoincenterCommand& CoincenterCommand::setPercentageAmount(bool value) {
  if (_type != CoincenterCommandType::kBuy && _type != CoincenterCommandType::kSell &&
      _type != CoincenterCommandType::kTrade && _type != CoincenterCommandType::kWithdraw) {
    throw exception("Percentage trade can only be set for trade / buy / sell or withdraw command");
  }
  _isPercentageAmount = value;
  return *this;
}
}  // namespace cct