#include "coincentercommand.hpp"

#include <utility>

#include "cct_exception.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "exchange-names.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {
namespace {
bool IsOrderCommand(CoincenterCommandType cmd) {
  switch (cmd) {
    case CoincenterCommandType::kOrdersCancel:  // NOLINT(bugprone-branch-clone)
      [[fallthrough]];
    case CoincenterCommandType::kOrdersClosed:
      [[fallthrough]];
    case CoincenterCommandType::kOrdersOpened:
      return true;
    default:
      return false;
  }
}
}  // namespace

CoincenterCommand& CoincenterCommand::setExchangeNames(ExchangeNames exchangeNames) {
  _exchangeNames = std::move(exchangeNames);
  return *this;
}

CoincenterCommand& CoincenterCommand::setOrdersConstraints(OrdersConstraints ordersConstraints) {
  if (!IsOrderCommand(_type)) {
    throw exception("Order constraints can only be used for orders related commands");
  }
  _specialOptions = std::move(ordersConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setDepositsConstraints(DepositsConstraints depositsConstraints) {
  if (_type != CoincenterCommandType::kRecentDeposits) {
    throw exception("Deposit constraints can only be used for deposits related commands");
  }
  _specialOptions = std::move(depositsConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setWithdrawsConstraints(WithdrawsConstraints withdrawsConstraints) {
  if (_type != CoincenterCommandType::kRecentWithdraws) {
    throw exception("Withdraw constraints can only be used for withdraws related commands");
  }
  _specialOptions = std::move(withdrawsConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setTradeOptions(TradeOptions tradeOptions) {
  if (_type != CoincenterCommandType::kBuy && _type != CoincenterCommandType::kSell &&
      _type != CoincenterCommandType::kTrade) {
    throw exception("Trade options can only be used for trade related commands");
  }
  _specialOptions = std::move(tradeOptions);
  return *this;
}

CoincenterCommand& CoincenterCommand::setWithdrawOptions(WithdrawOptions withdrawOptions) {
  if (_type != CoincenterCommandType::kWithdrawApply) {
    throw exception("Withdraw options can only be used for withdraws");
  }
  _specialOptions = std::move(withdrawOptions);
  return *this;
}

CoincenterCommand& CoincenterCommand::setAmount(MonetaryAmount amount) {
  _amount = amount;
  return *this;
}

CoincenterCommand& CoincenterCommand::setDepth(int depth) {
  _n = depth;
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
  if (_cur1.isNeutral() && !cur2.isNeutral()) {
    throw exception("First currency should be set before second one");
  }
  _cur2 = cur2;
  return *this;
}

CoincenterCommand& CoincenterCommand::setPercentageAmount(bool value) {
  if (_type != CoincenterCommandType::kBuy && _type != CoincenterCommandType::kSell &&
      _type != CoincenterCommandType::kTrade && _type != CoincenterCommandType::kWithdrawApply) {
    throw exception("Percentage trade can only be set for trade / buy / sell or withdraw command");
  }
  _isPercentageAmount = value;
  return *this;
}

CoincenterCommand& CoincenterCommand::withBalanceInUse(bool value) {
  if (_type != CoincenterCommandType::kBalance) {
    throw exception("With balance in use can only be set for Balance command");
  }
  _withBalanceInUse = value;
  return *this;
}
}  // namespace cct
