#pragma once

#include <ostream>

#include "cct_const.hpp"
#include "cct_json.hpp"
#include "queryresulttypes.hpp"

namespace cct {
class BalancePerExchangePortfolio {
 public:
  explicit BalancePerExchangePortfolio(const BalancePerExchange &balancePerExchange)
      : _balancePerExchange(balancePerExchange) {}

  /// Pretty print table of balance.
  /// @param wide if true, all exchange amount will be printed as well
  void printTable(std::ostream &os, bool wide) const;

  /// Print in json format.
  json printJson(CurrencyCode equiCurrency) const;

 private:
  BalancePortfolio computeTotal() const;

  const BalancePerExchange &_balancePerExchange;
};

}  // namespace cct