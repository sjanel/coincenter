#include "balanceportfolio.hpp"

#include <algorithm>

#include "cct_fixedcapacityvector.hpp"
#include "cct_variadictable.hpp"

namespace cct {
void BalancePortfolio::add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency) {
  MonetaryAmountWithEquivalent elem{amount, equivalentInMainCurrency};
  auto sameCurrencyIt = _monetaryAmountSet.find(elem);
  if (sameCurrencyIt == _monetaryAmountSet.end()) {
    _monetaryAmountSet.insert(elem);
  } else {
    auto nh = _monetaryAmountSet.extract(sameCurrencyIt);
    nh.value().amount += amount;
    nh.value().equi += equivalentInMainCurrency;
    _monetaryAmountSet.insert(std::move(nh));
  }
}

void BalancePortfolio::merge(const BalancePortfolio &o) {
  for (const MonetaryAmountWithEquivalent &monetaryAmountWithEqui : o) {
    add(monetaryAmountWithEqui.amount, monetaryAmountWithEqui.equi);
  }
}

MonetaryAmount BalancePortfolio::getBalance(CurrencyCode currencyCode) const {
  auto it = std::partition_point(
      _monetaryAmountSet.begin(), _monetaryAmountSet.end(),
      [currencyCode](const MonetaryAmountWithEquivalent &a) { return a.amount.currencyCode() < currencyCode; });
  if (it == _monetaryAmountSet.end() || it->amount.currencyCode() != currencyCode) {
    return MonetaryAmount(0, currencyCode, 0);
  }
  return it->amount;
}

void BalancePortfolio::print(std::ostream &os) const {
  if (empty()) {
    os << "No Balance to display" << std::endl;
  } else {
    CurrencyCode balanceCurrencyCode = _monetaryAmountSet.front().equi.currencyCode();
    if (balanceCurrencyCode != CurrencyCode::kNeutral) {
      MonetaryAmount totalSum = MonetaryAmount("0", balanceCurrencyCode);
      cct::FixedCapacityVector<std::string, 3> cols;
      cols.emplace_back("Amount");
      cols.emplace_back("Currency");
      cols.emplace_back("Eq. (").append(balanceCurrencyCode.str()).push_back(')');
      VariadicTable<std::string, std::string, std::string> vt(cols);
      for (const auto &m : convertToSortedByAmountVector()) {
        vt.addRow(m.amount.amountStr(), std::string(m.amount.currencyCode().str()),
                  m.equi.isZero() ? "???" : m.equi.amountStr());
        totalSum += m.equi;
      }
      vt.print(os);
      os << "* Total: " << totalSum << std::endl;
    } else {
      std::array<std::string, 2> cols = {"Amount", "Currency"};
      VariadicTable<std::string, std::string> vt(cols);
      for (const auto &m : *this) {
        vt.addRow(m.amount.amountStr(), std::string(m.amount.currencyCode().str()));
      }
      vt.print(os);
    }
  }
}

BalancePortfolio::MonetaryAmountVec BalancePortfolio::convertToSortedByAmountVector() const {
  MonetaryAmountVec ret(_monetaryAmountSet.begin(), _monetaryAmountSet.end());
  std::sort(ret.begin(), ret.end(),
            [](const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) {
              return lhs.equi > rhs.equi;
            });
  return ret;
}
}  // namespace cct