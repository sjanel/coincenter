#include "balanceperexchangeportfolio.hpp"

#include "cct_string.hpp"
#include "simpletable.hpp"

namespace cct {

void BalancePerExchangePortfolio::add(ExchangeName exchangeName, BalancePortfolio balancePortfolio) {
  _balances.front() += balancePortfolio;
  _balances.push_back(std::move(balancePortfolio));
  _exchanges.push_back(std::move(exchangeName));
}

void BalancePerExchangePortfolio::print(std::ostream &os, bool wide) const {
  BalancePortfolio total = _balances.front();
  if (total.empty()) {
    os << "No Balance to display" << std::endl;
  } else {
    CurrencyCode balanceCurrencyCode = total.front().equi.currencyCode();
    const bool countEqui = !balanceCurrencyCode.isNeutral();
    SimpleTable balanceTable;
    SimpleTable::Row header("Currency", "Total amount on selected");

    if (countEqui) {
      total.sortByDecreasingEquivalentAmount();

      string balanceEqCur("Total ");
      balanceEqCur.append(balanceCurrencyCode.str()).append(" eq");
      header.emplace_back(std::move(balanceEqCur));
    }

    if (wide) {
      for (const ExchangeName &e : _exchanges) {
        string account(e.name());
        account.push_back('_');
        account.append(e.keyName());
        header.emplace_back(std::move(account));
      }
    }
    balanceTable.push_back(std::move(header));

    MonetaryAmount totalSum(0, balanceCurrencyCode);
    const int nbExchanges = _exchanges.size();
    for (const auto &[amount, equi] : total) {
      // Amounts impossible to convert have a zero value
      SimpleTable::Row r(amount.currencyStr(), amount.amountStr());
      if (countEqui) {
        r.emplace_back(equi.amountStr());
        totalSum += equi;
      }
      if (wide) {
        for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
          r.emplace_back(_balances[1 + exchangePos].get(amount.currencyCode()).amountStr());
        }
      }
      balanceTable.push_back(std::move(r));
    }
    balanceTable.print(os);
    if (countEqui) {
      os << "* Total: " << totalSum << std::endl;
    }
  }
}

}  // namespace cct