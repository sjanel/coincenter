#pragma once

#include <initializer_list>
#include <span>

#include "cct_type_traits.hpp"
#include "cct_vector.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {

namespace api {
class ExchangePrivate;
}

class BalancePortfolio {
 private:
  struct MonetaryAmountWithEquivalent {
    MonetaryAmount amount;
    MonetaryAmount equi;

    bool operator==(const MonetaryAmountWithEquivalent&) const noexcept = default;
  };

  using MonetaryAmountVec = vector<MonetaryAmountWithEquivalent>;

 public:
  using size_type = MonetaryAmountVec::size_type;

  BalancePortfolio() noexcept = default;

  BalancePortfolio(std::initializer_list<MonetaryAmount> init)
      : BalancePortfolio(std::span<const MonetaryAmount>(init.begin(), init.end())) {}

  BalancePortfolio(std::span<const MonetaryAmount> init);

  MonetaryAmount get(CurrencyCode currencyCode) const;

  bool hasSome(CurrencyCode cur) const { return get(cur) > 0; }
  bool hasAtLeast(MonetaryAmount amount) const { return get(amount.currencyCode()) >= amount; }

  auto begin() { return _sortedAmounts.begin(); }
  auto end() { return _sortedAmounts.end(); }

  auto begin() const { return _sortedAmounts.begin(); }
  auto end() const { return _sortedAmounts.end(); }

  auto cbegin() const { return _sortedAmounts.cbegin(); }
  auto cend() const { return _sortedAmounts.cend(); }

  bool empty() const noexcept { return _sortedAmounts.empty(); }

  size_type size() const noexcept { return _sortedAmounts.size(); }

  void reserve(size_type newCapacity) { _sortedAmounts.reserve(newCapacity); }

  CurrencyCode equiCurrency() const;

  void sortByDecreasingEquivalentAmount();

  /// Adds an amount in this portfolio without an equivalent amount.
  BalancePortfolio& operator+=(MonetaryAmount amount);

  /// Merge the amounts of two different portfolios.
  BalancePortfolio& operator+=(const BalancePortfolio& other);

  bool operator==(const BalancePortfolio&) const = default;

  using trivially_relocatable = is_trivially_relocatable<MonetaryAmountVec>::type;

 private:
  MonetaryAmountVec _sortedAmounts;
};

}  // namespace cct