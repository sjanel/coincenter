#pragma once

#include <ostream>

#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

class BalancePortfolio {
 public:
  struct MonetaryAmountWithEquivalent {
    MonetaryAmount amount;
    MonetaryAmount equi;
  };

 private:
  using MonetaryAmountVec = vector<MonetaryAmountWithEquivalent>;

 public:
  using const_iterator = MonetaryAmountVec::const_iterator;
  using size_type = MonetaryAmountVec::size_type;

  /// Adds an amount in the `BalancePortfolio`.
  /// @param equivalentInMainCurrency (optional) also add its corresponding value in another currency
  void add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency = MonetaryAmount());

  void add(const BalancePortfolio &o);

  MonetaryAmount getBalance(CurrencyCode currencyCode) const;

  const_iterator begin() const { return _sortedAmounts.begin(); }
  const_iterator end() const { return _sortedAmounts.end(); }

  void print(std::ostream &os) const;

  bool empty() const noexcept { return _sortedAmounts.empty(); }

  size_type size() const noexcept { return _sortedAmounts.size(); }

 private:
  MonetaryAmountVec convertToSortedByAmountVector() const;

  MonetaryAmountVec _sortedAmounts;
};
}  // namespace cct