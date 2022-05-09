#pragma once

#include <ostream>

#include "balanceportfolio.hpp"
#include "cct_const.hpp"
#include "cct_smallvector.hpp"
#include "exchangename.hpp"

namespace cct {
class BalancePerExchangePortfolio {
 public:
  void add(ExchangeName exchangeName, BalancePortfolio balancePortfolio);

  /// Pretty print table of balance.
  /// @param wide if true, all exchange amount will be printed as well
  void print(std::ostream &os, bool wide) const;

 private:
  // +1 for total in first position
  using BalancePortfolioVector = SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts + 1>;

  BalancePortfolioVector _balances{1};
  ExchangeNames _exchanges;
};

}  // namespace cct