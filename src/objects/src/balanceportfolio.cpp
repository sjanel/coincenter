#include "balanceportfolio.hpp"

#include <algorithm>
#include <span>

#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
namespace {
inline auto &operator+=(auto &lhs, const auto &rhs) {
  lhs.amount += rhs.amount;
  lhs.equi += rhs.equi;

  return lhs;
}

}  // namespace

BalancePortfolio::BalancePortfolio(std::span<const MonetaryAmount> init) {
  // Simple for loop to avoid complex code eliminating duplicates for same currency
  // TODO (for fun): we could maybe replace this with a more elegant std::accumulate algorithm
  std::ranges::for_each(init, [this](const auto &am) { *this += am; });
}

BalancePortfolio &BalancePortfolio::operator+=(MonetaryAmount amount) {
  if (amount != 0) {
    auto isCurrencyCodeLt = [amount](const auto &elem) { return elem.amount.currencyCode() < amount.currencyCode(); };
    auto lb = std::ranges::partition_point(_sortedAmounts, isCurrencyCodeLt);

    if (lb == _sortedAmounts.end()) {
      _sortedAmounts.emplace_back(amount, MonetaryAmount());
    } else if (lb->amount.currencyCode() != amount.currencyCode()) {
      _sortedAmounts.emplace(lb, amount, MonetaryAmount());
    } else {
      // equal currencies, sum amounts
      lb->amount += amount;
    }
  }
  return *this;
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
  auto amountCurrencyCompare = [](const auto &lhs, const auto &rhs) {
    return lhs.amount.currencyCode() < rhs.amount.currencyCode();
  };

  while (first2 != last2) {
    if (first1 == last1) {
      _sortedAmounts.insert(_sortedAmounts.end(), first2, last2);
      break;
    }
    if (amountCurrencyCompare(*first1, *first2)) {
      ++first1;
    } else if (amountCurrencyCompare(*first2, *first1)) {
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
  std::ranges::sort(_sortedAmounts, [](const auto &lhs, const auto &rhs) {
    if (lhs.equi != rhs.equi) {
      return lhs.equi > rhs.equi;
    }
    return lhs.amount.currencyCode() < rhs.amount.currencyCode();
  });
}

CurrencyCode BalancePortfolio::equiCurrency() const {
  if (_sortedAmounts.empty()) {
    return {};
  }
  return _sortedAmounts.front().equi.currencyCode();
}

}  // namespace cct