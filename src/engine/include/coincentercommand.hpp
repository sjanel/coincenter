#pragma once

#include <optional>
#include <type_traits>
#include <variant>

#include "cct_type_traits.hpp"
#include "coincentercommandtype.hpp"
#include "currencycode.hpp"
#include "depositsconstraints.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "tradeoptions.hpp"
#include "withdrawoptions.hpp"
#include "withdrawsconstraints.hpp"
#include "withdrawsordepositsconstraints.hpp"

namespace cct {
class CoincenterCommand {
 public:
  explicit CoincenterCommand(CoincenterCommandType type) : _type(type) {}

  CoincenterCommand& setExchangeNames(const ExchangeNames& exchangeNames);
  CoincenterCommand& setExchangeNames(ExchangeNames&& exchangeNames);

  CoincenterCommand& setOrdersConstraints(const OrdersConstraints& ordersConstraints);
  CoincenterCommand& setOrdersConstraints(OrdersConstraints&& ordersConstraints);

  CoincenterCommand& setDepositsConstraints(const DepositsConstraints& depositsConstraints);
  CoincenterCommand& setDepositsConstraints(DepositsConstraints&& depositsConstraints);

  CoincenterCommand& setWithdrawsConstraints(const WithdrawsConstraints& withdrawsConstraints);
  CoincenterCommand& setWithdrawsConstraints(WithdrawsConstraints&& withdrawsConstraints);

  CoincenterCommand& setTradeOptions(const TradeOptions& tradeOptions);
  CoincenterCommand& setTradeOptions(TradeOptions&& tradeOptions);

  CoincenterCommand& setWithdrawOptions(const WithdrawOptions& withdrawOptions);
  CoincenterCommand& setWithdrawOptions(WithdrawOptions&& withdrawOptions);

  CoincenterCommand& setAmount(MonetaryAmount amount);

  CoincenterCommand& setDepth(int depth);
  CoincenterCommand& setNbLastTrades(int nbTrades) { return setDepth(nbTrades); }

  CoincenterCommand& setMarket(Market market);

  CoincenterCommand& setCur1(CurrencyCode cur1);
  CoincenterCommand& setCur2(CurrencyCode cur2);

  CoincenterCommand& setPercentageAmount(bool value = true);
  CoincenterCommand& withBalanceInUse(bool value = true);

  const ExchangeNames& exchangeNames() const { return _exchangeNames; }

  const OrdersConstraints& ordersConstraints() const { return std::get<OrdersConstraints>(_specialOptions); }

  const WithdrawsOrDepositsConstraints& withdrawsOrDepositsConstraints() const {
    return std::get<WithdrawsOrDepositsConstraints>(_specialOptions);
  }

  const TradeOptions& tradeOptions() const { return std::get<TradeOptions>(_specialOptions); }

  const WithdrawOptions& withdrawOptions() const { return std::get<WithdrawOptions>(_specialOptions); }

  MonetaryAmount amount() const { return _amount; }

  int nbLastTrades() const { return _n; }
  std::optional<int> optDepth() const { return _n == -1 ? std::nullopt : std::optional<int>(_n); }

  Market market() const { return _market; }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  CoincenterCommandType type() const { return _type; }

  bool isPercentageAmount() const { return _isPercentageAmount; }
  bool withBalanceInUse() const { return _withBalanceInUse; }

  bool operator==(const CoincenterCommand&) const noexcept = default;

  using trivially_relocatable = std::integral_constant<bool, is_trivially_relocatable_v<ExchangeNames> &&
                                                                 is_trivially_relocatable_v<OrdersConstraints>>::type;

 private:
  using SpecialOptions = std::variant<OrdersConstraints, WithdrawsOrDepositsConstraints, TradeOptions, WithdrawOptions>;

  ExchangeNames _exchangeNames;
  SpecialOptions _specialOptions;
  MonetaryAmount _amount;
  Market _market;
  CurrencyCode _cur1, _cur2;
  int _n = -1;
  CoincenterCommandType _type;
  bool _isPercentageAmount = false;
  bool _withBalanceInUse = false;
};

}  // namespace cct