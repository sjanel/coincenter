#include "balanceportfolio.hpp"

#include <algorithm>

#include "cct_fixedcapacityvector.hpp"
#include "cct_variadictable.hpp"

namespace cct {
namespace {
using MonetaryAmountWithEquivalent = BalancePortfolio::MonetaryAmountWithEquivalent;

inline bool Compare(const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) {
  return lhs.amount.currencyCode() < rhs.amount.currencyCode();
}

inline MonetaryAmountWithEquivalent &operator+=(MonetaryAmountWithEquivalent &lhs,
                                                const MonetaryAmountWithEquivalent &rhs) {
  lhs.amount += rhs.amount;
  lhs.equi += rhs.equi;
  return lhs;
}
}  // namespace

void BalancePortfolio::add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency) {
  MonetaryAmountWithEquivalent elem{amount, equivalentInMainCurrency};
  auto lb = std::lower_bound(_sortedAmounts.begin(), _sortedAmounts.end(), elem, Compare);
  if (lb == _sortedAmounts.end()) {
    _sortedAmounts.push_back(std::move(elem));
  } else if (Compare(elem, *lb)) {
    _sortedAmounts.insert(lb, std::move(elem));
  } else {
    // equal, sum amounts
    *lb += elem;
  }
}

void BalancePortfolio::add(const BalancePortfolio &o) {
  auto first1 = _sortedAmounts.begin(), last1 = _sortedAmounts.end();
  auto first2 = o.begin(), last2 = o.end();

  while (first2 != last2) {
    if (first1 == last1) {
      _sortedAmounts.insert(_sortedAmounts.end(), first2, last2);
      break;
    }
    if (Compare(*first1, *first2)) {
      ++first1;
    } else if (Compare(*first2, *first1)) {
      first1 = _sortedAmounts.insert(first1, *first2);
      ++first1;
      last1 = _sortedAmounts.end();  // as iterators may have been invalidated
      ++first2;
    } else {
      // equal
      *first1 += *first2;
      ++first1;
      ++first2;
    }
  }
}

MonetaryAmount BalancePortfolio::getBalance(CurrencyCode currencyCode) const {
  auto it = std::partition_point(
      _sortedAmounts.begin(), _sortedAmounts.end(),
      [currencyCode](const MonetaryAmountWithEquivalent &a) { return a.amount.currencyCode() < currencyCode; });
  if (it == _sortedAmounts.end() || it->amount.currencyCode() != currencyCode) {
    return MonetaryAmount(0, currencyCode);
  }
  return it->amount;
}

void BalancePortfolio::print(std::ostream &os) const {
  if (empty()) {
    os << "No Balance to display" << std::endl;
  } else {
    CurrencyCode balanceCurrencyCode = _sortedAmounts.front().equi.currencyCode();
    if (balanceCurrencyCode != CurrencyCode::kNeutral) {
      MonetaryAmount totalSum = MonetaryAmount("0", balanceCurrencyCode);
      FixedCapacityVector<string, 3> cols;
      cols.emplace_back("Amount");
      cols.emplace_back("Currency");
      cols.emplace_back("Eq. (").append(balanceCurrencyCode.str()).push_back(')');
      VariadicTable<string, string, string> vt(cols);
      for (const auto &m : convertToSortedByAmountVector()) {
        // Amounts impossible to convert have a zero value
        vt.addRow(m.amount.amountStr(), string(m.amount.currencyCode().str()), m.equi.amountStr());
        totalSum += m.equi;
      }
      vt.print(os);
      os << "* Total: " << totalSum << std::endl;
    } else {
      std::array<string, 2> cols = {"Amount", "Currency"};
      VariadicTable<string, string> vt(cols);
      for (const auto &m : *this) {
        vt.addRow(m.amount.amountStr(), string(m.amount.currencyCode().str()));
      }
      vt.print(os);
    }
  }
}

BalancePortfolio::MonetaryAmountVec BalancePortfolio::convertToSortedByAmountVector() const {
  MonetaryAmountVec ret = _sortedAmounts;
  std::sort(ret.begin(), ret.end(),
            [](const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) {
              return lhs.equi > rhs.equi;
            });
  return ret;
}
}  // namespace cct