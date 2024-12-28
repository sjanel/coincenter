#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>

#include "cct_type_traits.hpp"
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
#include "withdrawsordepositsconstraints.hpp"

namespace cct {
class CoincenterCommand {
 public:
  explicit CoincenterCommand(CoincenterCommandType type) : _type(type) {}

  CoincenterCommand& setExchangeNames(ExchangeNames exchangeNames);

  CoincenterCommand& setOrdersConstraints(OrdersConstraints ordersConstraints);

  CoincenterCommand& setDepositsConstraints(DepositsConstraints depositsConstraints);

  CoincenterCommand& setWithdrawsConstraints(WithdrawsConstraints withdrawsConstraints);

  CoincenterCommand& setTradeOptions(TradeOptions tradeOptions);

  CoincenterCommand& setWithdrawOptions(WithdrawOptions withdrawOptions);

  CoincenterCommand& setAmount(MonetaryAmount amount);

  CoincenterCommand& setDepth(int32_t depth);

  CoincenterCommand& setMarket(Market market);

  CoincenterCommand& setCur1(CurrencyCode cur1);
  CoincenterCommand& setCur2(CurrencyCode cur2);

  CoincenterCommand& setReplayOptions(ReplayOptions replayOptions);

  CoincenterCommand& setJsonConfigFile(std::string_view jsonConfigFile);

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

  int depth() const { return _n; }
  auto optDepth() const { return _n == -1 ? std::nullopt : std::optional<int>(_n); }

  Market market() const { return _market; }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  CoincenterCommandType type() const { return _type; }

  bool isPercentageAmount() const { return _isPercentageAmount; }
  bool withBalanceInUse() const { return _withBalanceInUse; }

  const ReplayOptions& replayOptions() const { return std::get<ReplayOptions>(_specialOptions); }

  std::string_view getJsonConfigFile() const { return std::get<std::string_view>(_specialOptions); }

  bool operator==(const CoincenterCommand&) const noexcept = default;

  using trivially_relocatable =
      std::bool_constant<is_trivially_relocatable_v<ExchangeNames> && is_trivially_relocatable_v<OrdersConstraints> &&
                         is_trivially_relocatable_v<WithdrawsOrDepositsConstraints> &&
                         is_trivially_relocatable_v<TradeOptions> && is_trivially_relocatable_v<WithdrawOptions> &&
                         is_trivially_relocatable_v<ReplayOptions>>::type;

 private:
  using SpecialOptions = std::variant<std::monostate, OrdersConstraints, WithdrawsOrDepositsConstraints, TradeOptions,
                                      WithdrawOptions, ReplayOptions, std::string_view>;

  ExchangeNames _exchangeNames;
  SpecialOptions _specialOptions;
  MonetaryAmount _amount;
  Market _market;
  CurrencyCode _cur1, _cur2;
  int32_t _n = -1;
  CoincenterCommandType _type;
  bool _isPercentageAmount = false;
  bool _withBalanceInUse = false;
};

}  // namespace cct
