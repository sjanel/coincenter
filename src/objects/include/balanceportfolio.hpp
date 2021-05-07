#pragma once

#include <ostream>

#include "cct_flatset.hpp"
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

  struct CompareMonetaryAmountWithEquivalentByCurrencyCode {
    bool operator()(const MonetaryAmountWithEquivalent &lhs, const MonetaryAmountWithEquivalent &rhs) const {
      return lhs.amount.currencyCode() < rhs.amount.currencyCode();
    }
  };

  using MonetaryAmountSet =
      cct::FlatSet<MonetaryAmountWithEquivalent, CompareMonetaryAmountWithEquivalentByCurrencyCode>;

  void add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency = MonetaryAmount());

  void merge(const BalancePortfolio &o);

  MonetaryAmount getBalance(CurrencyCode currencyCode) const;

  MonetaryAmountSet::const_iterator begin() const { return _monetaryAmountSet.begin(); }
  MonetaryAmountSet::const_iterator end() const { return _monetaryAmountSet.end(); }

  void print(std::ostream &os) const;

  bool empty() const { return _monetaryAmountSet.empty(); }

 private:
  using MonetaryAmountVec = cct::vector<MonetaryAmountWithEquivalent>;

  MonetaryAmountVec convertToSortedByAmountVector() const;

  MonetaryAmountSet _monetaryAmountSet;
};
}  // namespace cct