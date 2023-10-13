#include "balanceperexchangeportfolio.hpp"

#include <utility>

#include "balanceportfolio.hpp"
#include "cct_json.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "exchange.hpp"
#include "monetaryamount.hpp"
#include "simpletable.hpp"

namespace cct {

namespace {
MonetaryAmount ComputeTotalSum(const BalancePortfolio &total) {
  CurrencyCode balanceCurrencyCode = total.empty() ? CurrencyCode() : total.front().equi.currencyCode();
  MonetaryAmount totalSum(0, balanceCurrencyCode);
  for (const auto &[amount, equi] : total) {
    totalSum += equi;
  }
  return totalSum;
}
}  // namespace

SimpleTable BalancePerExchangePortfolio::getTable(bool wide) const {
  BalancePortfolio total = computeTotal();
  CurrencyCode balanceCurrencyCode = total.empty() ? CurrencyCode() : total.front().equi.currencyCode();
  const bool countEqui = !balanceCurrencyCode.isNeutral();
  SimpleTable::Row header("Currency", "Total amount on selected");

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
    SimpleTable::Row row(amount.currencyStr(), amount.amountStr());
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
    balanceTable.push_back(SimpleTable::Row::kDivider);
    SimpleTable::Row row("Total", "", ComputeTotalSum(total).amountStr());
    if (wide) {
      for (int exchangePos = 0; exchangePos < nbExchanges; ++exchangePos) {
        row.emplace_back("");
      }
    }
    balanceTable.push_back(std::move(row));
  }
  return balanceTable;
}

namespace {
json JsonForBalancePortfolio(const BalancePortfolio &balancePortfolio, CurrencyCode equiCurrency) {
  json ret = json::object();
  for (const auto &[amount, equiAmount] : balancePortfolio) {
    json curData;
    curData.emplace("a", amount.amountStr());
    if (!equiCurrency.isNeutral()) {
      curData.emplace("eq", equiAmount.amountStr());
    }
    ret.emplace(amount.currencyStr(), std::move(curData));
  }
  return ret;
}
}  // namespace

json BalancePerExchangePortfolio::printJson(CurrencyCode equiCurrency) const {
  json exchangePart = json::object();
  for (const auto &[exchangePtr, balancePortfolio] : _balancePerExchange) {
    auto it = exchangePart.find(exchangePtr->name());
    if (it == exchangePart.end()) {
      json balancePerExchangeData;
      balancePerExchangeData.emplace(exchangePtr->keyName(), JsonForBalancePortfolio(balancePortfolio, equiCurrency));
      exchangePart.emplace(exchangePtr->name(), std::move(balancePerExchangeData));
    } else {
      it->emplace(exchangePtr->keyName(), JsonForBalancePortfolio(balancePortfolio, equiCurrency));
    }
  }

  BalancePortfolio total = computeTotal();
  json totalPart;
  totalPart.emplace("cur", JsonForBalancePortfolio(total, equiCurrency));
  if (!equiCurrency.isNeutral()) {
    totalPart.emplace("eq", ComputeTotalSum(total).amountStr());
  }
  json out;
  out.emplace("exchange", std::move(exchangePart));
  out.emplace("total", std::move(totalPart));
  return out;
}

BalancePortfolio BalancePerExchangePortfolio::computeTotal() const {
  BalancePortfolio total;
  for (const auto &[exchangePtr, balancePortfolio] : _balancePerExchange) {
    total += balancePortfolio;
  }
  return total;
}

}  // namespace cct