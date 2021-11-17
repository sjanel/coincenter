#include "balanceperexchangeportfolio.hpp"

#include "cct_string.hpp"
#include "cct_variadictable.hpp"

namespace cct {
void BalancePerExchangePortfolio::add(const Exchange &exchange, const BalancePortfolio &balancePortfolio) {
  _balances.front() += balancePortfolio;
  _balances.push_back(balancePortfolio);
  _exchanges.push_back(std::addressof(exchange));
}

void BalancePerExchangePortfolio::add(const Exchange &exchange, BalancePortfolio &&balancePortfolio) {
  _balances.front() += balancePortfolio;
  _balances.push_back(std::move(balancePortfolio));
  _exchanges.push_back(std::addressof(exchange));
}

void BalancePerExchangePortfolio::print(std::ostream &os) const {
  BalancePortfolio total = _balances.front();
  if (total.empty()) {
    os << "No Balance to display" << std::endl;
  } else {
    CurrencyCode balanceCurrencyCode = total.front().equi.currencyCode();
    const bool countEqui = balanceCurrencyCode != CurrencyCode::kNeutral;
    if (countEqui) {
      FixedCapacityVector<string, 3> cols;
      cols.emplace_back("Currency");
      cols.emplace_back("Amount");
      cols.emplace_back(balanceCurrencyCode.str()).append(" eq");

      total.sortByDecreasingEquivalentAmount();

      VariadicTable<string, string, string> balanceTable(cols);

      MonetaryAmount totalSum(0, balanceCurrencyCode);
      for (const auto &[amount, equi] : total) {
        // Amounts impossible to convert have a zero value
        balanceTable.addRow(string(amount.currencyStr()), amount.amountStr(), equi.amountStr());
        totalSum += equi;
      }
      balanceTable.print(os);
      os << "* Total: " << totalSum << std::endl;
    } else {
      VariadicTable<string, string> balanceTable({"Currency", "Amount"});

      for (const auto &[amount, equi] : total) {
        // Amounts impossible to convert have a zero value
        balanceTable.addRow(string(amount.currencyStr()), amount.amountStr());
      }
      balanceTable.print(os);
    }
  }
}

}  // namespace cct