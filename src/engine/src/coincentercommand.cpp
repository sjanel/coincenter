#include "coincentercommand.hpp"

#include <cstdint>
#include <utility>

#include "cct_exception.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "exchange-names.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "replay-options.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"

namespace cct {
namespace {
bool IsOrderCommand(CoincenterCommandType cmd) {
  switch (cmd) {
    case CoincenterCommandType::OrdersCancel:  // NOLINT(bugprone-branch-clone)
      [[fallthrough]];
    case CoincenterCommandType::OrdersClosed:
      [[fallthrough]];
    case CoincenterCommandType::OrdersOpened:
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
  if (_type != CoincenterCommandType::RecentDeposits) {
    throw exception("Deposit constraints can only be used for deposits related commands");
  }
  _specialOptions = std::move(depositsConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setWithdrawsConstraints(WithdrawsConstraints withdrawsConstraints) {
  if (_type != CoincenterCommandType::RecentWithdraws) {
    throw exception("Withdraw constraints can only be used for withdraws related commands");
  }
  _specialOptions = std::move(withdrawsConstraints);
  return *this;
}

CoincenterCommand& CoincenterCommand::setTradeOptions(TradeOptions tradeOptions) {
  if (_type != CoincenterCommandType::Buy && _type != CoincenterCommandType::Sell &&
      _type != CoincenterCommandType::Trade) {
    throw exception("Trade options can only be used for trade related commands");
  }
  _specialOptions = std::move(tradeOptions);
  return *this;
}

CoincenterCommand& CoincenterCommand::setWithdrawOptions(WithdrawOptions withdrawOptions) {
  if (_type != CoincenterCommandType::Withdraw) {
    throw exception("Withdraw options can only be used for withdraws");
  }
  _specialOptions = std::move(withdrawOptions);
  return *this;
}

CoincenterCommand& CoincenterCommand::setAmount(MonetaryAmount amount) {
  _amount = amount;
  return *this;
}

CoincenterCommand& CoincenterCommand::setDepth(int32_t depth) {
  if (depth < 1) {
    throw exception("Depth cannot be less than 1");
  }
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
  if (_type != CoincenterCommandType::Buy && _type != CoincenterCommandType::Sell &&
      _type != CoincenterCommandType::Trade && _type != CoincenterCommandType::Withdraw) {
    throw exception("Percentage trade can only be set for trade / buy / sell or withdraw command");
  }
  _isPercentageAmount = value;
  return *this;
}

CoincenterCommand& CoincenterCommand::withBalanceInUse(bool value) {
  if (_type != CoincenterCommandType::Balance) {
    throw exception("With balance in use can only be set for Balance command");
  }
  _withBalanceInUse = value;
  return *this;
}

CoincenterCommand& CoincenterCommand::setReplayOptions(ReplayOptions replayOptions) {
  _specialOptions = std::move(replayOptions);
  return *this;
}

}  // namespace cct
