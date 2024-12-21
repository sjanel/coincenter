#include "balanceperexchangeportfolio.hpp"

#include <utility>

#include "balanceportfolio.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchange.hpp"
#include "monetaryamount.hpp"
#include "simpletable.hpp"

namespace cct {

namespace {
MonetaryAmount ComputeTotalSum(const BalancePortfolio &total) {
  CurrencyCode balanceCurrencyCode = total.equiCurrency();
  MonetaryAmount totalSum(0, balanceCurrencyCode);
  for (const auto &[amount, equi] : total) {
    totalSum += equi;
  }
  return totalSum;
}
}  // namespace

SimpleTable BalancePerExchangePortfolio::getTable(bool wide) const {
  BalancePortfolio total = computeTotal();
  CurrencyCode balanceCurrencyCode = total.equiCurrency();
  const bool countEqui = !balanceCurrencyCode.isNeutral();
  table::Row header("Currency", "Total amount on selected");

  if (countEqui) {
    total.sortByDecreasingEquivalentAmount();

    string balanceEqCur("Total ");
    balanceCurrencyCode.appendStrTo(balanceEqCur);
    balanceEqCur.append(" eq");
    header.emplace_back(std::move(balanceEqCur));
  }

  if (wide) {
    for (const auto &[exchangePtr, balancePortfolio] : _balancePerExchange) {
      string account(exchangePtr->name());
      account.push_back('_');
      account.append(exchangePtr->keyName());
      header.emplace_back(std::move(account));
    }
  }
  SimpleTable balanceTable{std::move(header)};

  const int nbExchanges = _balancePerExchange.size();
  for (const auto &[amount, equi] : total) {
    // Amounts impossible to convert have a zero value
    table::Row row(amount.currencyStr(), amount.amountStr());
    if (countEqui) {
      row.emplace_back(equi.amountStr());
    }
    if (wide) {
      for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
        row.emplace_back(_balancePerExchange[exchangePos].second.get(amount.currencyCode()).amountStr());
      }
    }
    balanceTable.push_back(std::move(row));
  }
  if (countEqui) {
    balanceTable.emplace_back();
    table::Row row("Total", "", ComputeTotalSum(total).amountStr());
    if (wide) {
      for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
        row.emplace_back("");
      }
    }
    balanceTable.push_back(std::move(row));
  }
  return balanceTable;
}

BalancePortfolio BalancePerExchangePortfolio::computeTotal() const {
  BalancePortfolio total;
  for (const auto &[exchangePtr, balancePortfolio] : _balancePerExchange) {
    total += balancePortfolio;
  }
  return total;
}

}  // namespace cct