#pragma once

#include "queryresulttypes.hpp"
#include "simpletable.hpp"

namespace cct {
class BalancePerExchangePortfolio {
 public:
  explicit BalancePerExchangePortfolio(const BalancePerExchange &balancePerExchange)
      : _balancePerExchange(balancePerExchange) {}

  /// Pretty print table of balance.
  /// @param wide if true, all exchange amount will be printed as well
  SimpleTable getTable(bool wide) const;

 private:
  BalancePortfolio computeTotal() const;

  const BalancePerExchange &_balancePerExchange;
};

}  // namespace cct