#pragma once

#include <cstdint>
#include <variant>

#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"
#include "tradeoptions.hpp"

namespace cct {
class CoincenterCommand {
 public:
  enum class Type : int8_t {
    kMarkets,
    kConversionPath,
    kLastPrice,
    kTicker,
    kOrderbook,
    kLastTrades,
    kLast24hTradedVolume,
    kWithdrawFee,

    kBalance,
    kDepositInfo,
    kOrdersOpened,
    kOrdersCancel,
    kTrade,
    kBuy,
    kSell,
    kWithdraw,
  };

  explicit CoincenterCommand(Type type) : _type(type) {}

  CoincenterCommand& setExchangeNames(const ExchangeNames& exchangeNames);
  CoincenterCommand& setExchangeNames(ExchangeNames&& exchangeNames);

  CoincenterCommand& setOrdersConstraints(const OrdersConstraints& ordersConstraints);
  CoincenterCommand& setOrdersConstraints(OrdersConstraints&& ordersConstraints);

  CoincenterCommand& setTradeOptions(const TradeOptions& tradeOptions);
  CoincenterCommand& setTradeOptions(TradeOptions&& tradeOptions);

  CoincenterCommand& setAmount(MonetaryAmount amount);

  CoincenterCommand& setDepth(int d);
  CoincenterCommand& setNbLastTrades(int n) { return setDepth(n); }

  CoincenterCommand& setMarket(Market market);

  CoincenterCommand& setCur1(CurrencyCode cur1);
  CoincenterCommand& setCur2(CurrencyCode cur2);

  CoincenterCommand& setPercentageAmount(bool value = true);

  bool isPublic() const;
  bool isPrivate() const { return !isPublic(); }

  bool isReadOnly() const;
  bool isWrite() const { return !isReadOnly(); }

  const ExchangeNames& exchangeNames() const { return _exchangeNames; }

  const OrdersConstraints& ordersConstraints() const { return std::get<OrdersConstraints>(_tradeOrOrdersOptions); }

  const TradeOptions& tradeOptions() const { return std::get<TradeOptions>(_tradeOrOrdersOptions); }

  MonetaryAmount amount() const { return _amount; }

  int nbLastTrades() const { return _n; }
  std::optional<int> optDepth() const { return _n == -1 ? std::nullopt : std::optional<int>(_n); }

  Market market() const { return _market; }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  Type type() const { return _type; }

  bool isPercentageAmount() const { return _isPercentageAmount; }

  using trivially_relocatable = std::integral_constant<bool, is_trivially_relocatable_v<ExchangeNames> &&
                                                                 is_trivially_relocatable_v<OrdersConstraints>>::type;

 private:
  using TradeOrOrdersOptions = std::variant<OrdersConstraints, TradeOptions>;

  ExchangeNames _exchangeNames;
  TradeOrOrdersOptions _tradeOrOrdersOptions;
  MonetaryAmount _amount;
  int _n = -1;
  Market _market;
  CurrencyCode _cur1, _cur2;
  Type _type;
  bool _isPercentageAmount = false;
};

}  // namespace cct