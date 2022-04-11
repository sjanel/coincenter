#pragma once

#include <initializer_list>
#include <span>

#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

class BalancePortfolio {
 public:
  struct MonetaryAmountWithEquivalent {
    MonetaryAmount amount;
    MonetaryAmount equi;

    bool operator==(const MonetaryAmountWithEquivalent &) const = default;
  };

 private:
  using MonetaryAmountVec = vector<MonetaryAmountWithEquivalent>;

 public:
  using const_iterator = MonetaryAmountVec::const_iterator;
  using size_type = MonetaryAmountVec::size_type;

  BalancePortfolio() noexcept = default;

  BalancePortfolio(std::initializer_list<MonetaryAmount> init)
      : BalancePortfolio(std::span<const MonetaryAmount>(init.begin(), init.end())) {}

  BalancePortfolio(std::span<const MonetaryAmount> init);

  BalancePortfolio(std::initializer_list<MonetaryAmountWithEquivalent> init)
      : BalancePortfolio(std::span<const MonetaryAmountWithEquivalent>(init.begin(), init.end())) {}

  BalancePortfolio(std::span<const MonetaryAmountWithEquivalent> init);

  /// Adds an amount in the `BalancePortfolio`.
  /// @param equivalentInMainCurrency (optional) also add its corresponding value in another currency
  void add(MonetaryAmount amount, MonetaryAmount equivalentInMainCurrency = MonetaryAmount());

  MonetaryAmount get(CurrencyCode currencyCode) const;

  bool hasSome(CurrencyCode cur) const { return get(cur).isStrictlyPositive(); }
  bool hasAtLeast(MonetaryAmount amount) const { return get(amount.currencyCode()) >= amount; }

  const_iterator begin() const { return _sortedAmounts.begin(); }
  const_iterator end() const { return _sortedAmounts.end(); }

  MonetaryAmountWithEquivalent front() const { return _sortedAmounts.front(); }
  MonetaryAmountWithEquivalent back() const { return _sortedAmounts.back(); }

  bool empty() const noexcept { return _sortedAmounts.empty(); }

  size_type size() const noexcept { return _sortedAmounts.size(); }

  void sortByDecreasingEquivalentAmount();

  BalancePortfolio &operator+=(const BalancePortfolio &o);

  bool operator==(const BalancePortfolio &) const = default;

  using trivially_relocatable = is_trivially_relocatable<MonetaryAmountVec>::type;

 private:
  MonetaryAmountVec _sortedAmounts;
};
}  // namespace cct