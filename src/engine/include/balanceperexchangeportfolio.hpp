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

  void print(std::ostream &os) const;

 private:
  // +1 for total in first position
  using BalancePortfolioVector = SmallVector<BalancePortfolio, kTypicalNbPrivateAccounts + 1>;

  BalancePortfolioVector _balances;
  ConstExchangeRetriever::SelectedExchanges _exchanges;
};

}  // namespace cct