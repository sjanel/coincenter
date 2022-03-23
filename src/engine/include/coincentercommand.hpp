#pragma once

#include <cstdint>

#include "currencycode.hpp"
#include "exchangename.hpp"
#include "market.hpp"
#include "monetaryamount.hpp"
#include "ordersconstraints.hpp"

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

  CoincenterCommand& setAmount(MonetaryAmount amount);

  CoincenterCommand& setRepeats(int repeats);

  CoincenterCommand& setN(int n);

  CoincenterCommand& setMarket(Market market);

  CoincenterCommand& setCur1(CurrencyCode cur1);
  CoincenterCommand& setCur2(CurrencyCode cur2);

  CoincenterCommand& setPercentageTrade();

  bool isPublic() const;
  bool isPrivate() const { return !isPublic(); }

  bool isReadOnly() const;
  bool isWrite() const { return !isReadOnly(); }

  const ExchangeNames& exchangeNames() const { return _exchangeNames; }

  const OrdersConstraints& ordersConstraints() const { return _ordersConstraints; }

  MonetaryAmount amount() const { return _amount; }

  int nbRepeats() const { return _repeats; }

  int n() const { return _n; }

  Market market() const { return _market; }

  CurrencyCode cur1() const { return _cur1; }
  CurrencyCode cur2() const { return _cur2; }

  Type type() const { return _type; }

  bool isPercentageTrade() const { return _isPercentageTrade; }

  using trivially_relocatable = std::integral_constant<bool, is_trivially_relocatable_v<ExchangeNames> &&
                                                                 is_trivially_relocatable_v<OrdersConstraints>>::type;

 private:
  ExchangeNames _exchangeNames;
  OrdersConstraints _ordersConstraints;
  MonetaryAmount _amount;
  int _repeats = 1;
  int _n = 0;
  Market _market;
  CurrencyCode _cur1, _cur2;
  Type _type;
  bool _isPercentageTrade = false;
};

}  // namespace cct