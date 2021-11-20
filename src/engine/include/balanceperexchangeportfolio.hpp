#pragma once

#include <ostream>

#include "balanceportfolio.hpp"
#include "cct_const.hpp"
#include "cct_smallvector.hpp"
#include "exchangeretriever.hpp"

namespace cct {
class BalancePerExchangePortfolio {
 public:
  BalancePerExchangePortfolio() : _balances(1) {}

  void add(const Exchange &exchange, const BalancePortfolio &balancePortfolio);
  void add(const Exchange &exchange, BalancePortfolio &&balancePortfolio);

  /// Pretty print table of balance.
  /// @param wide if true, all exchange amount will be printed as well
  void print(std::ostream &os, bool wide) const;

 private:
  // +1 for total in first position
  using BalancePortfolioVector = SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts + 1>;

  BalancePortfolioVector _balances;
  ConstExchangeRetriever::SelectedExchanges _exchanges;
};

}  // namespace cct