#include "balanceportfolio.hpp"

#include <algorithm>

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
  auto lb = std::ranges::lower_bound(_sortedAmounts, elem, Compare);
  if (lb == _sortedAmounts.end()) {
    _sortedAmounts.push_back(std::move(elem));
  } else if (Compare(elem, *lb)) {
    _sortedAmounts.insert(lb, std::move(elem));
  } else {
    // equal, sum amounts
    *lb += elem;
  }
}

MonetaryAmount BalancePortfolio::get(CurrencyCode currencyCode) const {
  auto it = std::ranges::partition_point(_sortedAmounts, [currencyCode](const MonetaryAmountWithEquivalent &a) {
    return a.amount.currencyCode() < currencyCode;
  });
  if (it == _sortedAmounts.end() || it->amount.currencyCode() != currencyCode) {
    return MonetaryAmount(0, currencyCode);
  }
  return it->amount;
}

BalancePortfolio &BalancePortfolio::operator+=(const BalancePortfolio &o) {
  auto first1 = _sortedAmounts.begin();
  auto last1 = _sortedAmounts.end();
  auto first2 = o.begin();
  auto last2 = o.end();

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
  return *this;
}

void BalancePortfolio::sortByDecreasingEquivalentAmount() {
  std::ranges::sort(_sortedAmounts,
                    [](const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) {
                      if (lhs.equi != rhs.equi) {
                        return lhs.equi > rhs.equi;
                      }
                      return lhs.amount.currencyCode() < rhs.amount.currencyCode();
                    });
}
}  // namespace cct