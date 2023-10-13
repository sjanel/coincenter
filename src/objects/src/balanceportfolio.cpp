#include "balanceportfolio.hpp"

#include <algorithm>
#include <span>
#include <utility>

#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
namespace {
using MonetaryAmountWithEquivalent = BalancePortfolio::MonetaryAmountWithEquivalent;

inline bool CurCompare(const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) {
  return lhs.amount.currencyCode() < rhs.amount.currencyCode();
}

inline MonetaryAmountWithEquivalent &operator+=(MonetaryAmountWithEquivalent &lhs,
                                                const MonetaryAmountWithEquivalent &rhs) {
  lhs.amount += rhs.amount;
  lhs.equi += rhs.equi;
  return lhs;
}
}  // namespace

BalancePortfolio::BalancePortfolio(std::span<const MonetaryAmount> init) {
  // Simple for loop to avoid complex code eliminating duplicates for same currency
  for (MonetaryAmount ma : init) {
    add(ma);
  }
}

BalancePortfolio::BalancePortfolio(std::span<const MonetaryAmountWithEquivalent> init) {
  // Simple for loop to avoid complex code eliminating duplicates for same currency
  for (const MonetaryAmountWithEquivalent &monetaryAmountWithEquivalent : init) {
    add(monetaryAmountWithEquivalent.amount, monetaryAmountWithEquivalent.equi);
  }
}

void BalancePortfolio::add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency) {
  MonetaryAmountWithEquivalent elem{amount, equivalentInMainCurrency};
  auto lb = std::ranges::lower_bound(_sortedAmounts, elem, CurCompare);
  if (lb == _sortedAmounts.end()) {
    _sortedAmounts.push_back(std::move(elem));
  } else if (CurCompare(elem, *lb)) {
    _sortedAmounts.insert(lb, std::move(elem));
  } else {
    // equal, sum amounts
    *lb += std::move(elem);
  }
}

MonetaryAmount BalancePortfolio::get(CurrencyCode currencyCode) const {
  auto it = std::ranges::partition_point(
      _sortedAmounts, [currencyCode](const MonetaryAmountWithEquivalent &monetaryAmountWithEquivalent) {
        return monetaryAmountWithEquivalent.amount.currencyCode() < currencyCode;
      });
  if (it == _sortedAmounts.end() || it->amount.currencyCode() != currencyCode) {
    return MonetaryAmount(0, currencyCode);
  }
  return it->amount;
}

BalancePortfolio &BalancePortfolio::operator+=(const BalancePortfolio &other) {
  auto first1 = _sortedAmounts.begin();
  auto last1 = _sortedAmounts.end();
  auto first2 = other.begin();
  auto last2 = other.end();

  while (first2 != last2) {
    if (first1 == last1) {
      _sortedAmounts.insert(_sortedAmounts.end(), first2, last2);
      break;
    }
    if (CurCompare(*first1, *first2)) {
      ++first1;
    } else if (CurCompare(*first2, *first1)) {
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