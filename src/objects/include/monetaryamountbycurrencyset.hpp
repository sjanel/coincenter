#pragma once

#include <initializer_list>

#include "cct_flatset.hpp"
#include "cct_vector.hpp"
#include "monetaryamount.hpp"

namespace cct {

/// Helper type that can be used as input buffer for CurrencyExchangeFlatSet.
using MonetaryAmountVector = vector<MonetaryAmount>;

class MonetaryAmountByCurrencySet {
 private:
  struct CompareByCurrencyCode {
    bool operator()(MonetaryAmount lhs, MonetaryAmount rhs) const { return lhs.currencyCode() < rhs.currencyCode(); }
  };

  using SetType = FlatSet<MonetaryAmount, CompareByCurrencyCode>;

 public:
  using iterator = SetType::iterator;
  using const_iterator = SetType::const_iterator;
  using size_type = SetType::size_type;
  using value_type = SetType::value_type;

  MonetaryAmountByCurrencySet() noexcept = default;

  MonetaryAmountByCurrencySet(std::initializer_list<MonetaryAmount> init) : _set(init.begin(), init.end()) {}

  explicit MonetaryAmountByCurrencySet(MonetaryAmountVector &&vec) noexcept : _set(std::move(vec)) {}

  const MonetaryAmount &front() const { return _set.front(); }
  const MonetaryAmount &back() const { return _set.back(); }

  const_iterator begin() const noexcept { return _set.begin(); }
  const_iterator end() const noexcept { return _set.end(); }
  const_iterator cbegin() const noexcept { return _set.begin(); }
  const_iterator cend() const noexcept { return _set.end(); }

  bool empty() const noexcept { return _set.empty(); }
  size_type size() const noexcept { return _set.size(); }
  size_type max_size() const noexcept { return _set.max_size(); }

  size_type capacity() const noexcept { return _set.capacity(); }
  void reserve(size_type size) { _set.reserve(size); }

  void clear() noexcept { _set.clear(); }

  const_iterator find(const MonetaryAmount &v) const { return _set.find(v); }
  bool contains(const MonetaryAmount &v) const { return find(v) != end(); }

  const_iterator find(CurrencyCode standardCode) const {
    // This is possible as MonetaryAmount are ordered by standard code
    auto lbIt = std::lower_bound(_set.begin(), _set.end(), standardCode,
                                 [](MonetaryAmount lhs, CurrencyCode cur) { return lhs.currencyCode() < cur; });
    return lbIt == end() || standardCode < lbIt->currencyCode() ? end() : lbIt;
  }

  const MonetaryAmount &getOrThrow(CurrencyCode standardCode) const {
    const_iterator it = find(standardCode);
    if (it == _set.end()) {
      throw exception("Unknown currency code {}", standardCode);
    }
    return *it;
  }

  bool contains(CurrencyCode standardCode) const { return find(standardCode) != end(); }

  std::pair<iterator, bool> insert(const MonetaryAmount &v) { return _set.insert(v); }
  std::pair<iterator, bool> insert(MonetaryAmount &&v) { return _set.insert(std::move(v)); }

  iterator insert(const_iterator hint, const MonetaryAmount &v) { return _set.insert(hint, v); }
  iterator insert(const_iterator hint, MonetaryAmount &&v) { return _set.insert(hint, std::move(v)); }

  template <class InputIt>
  void insert(InputIt first, InputIt last) {
    _set.insert(first, last);
  }

  template <class... Args>
  std::pair<iterator, bool> emplace(Args &&...args) {
    return _set.emplace(std::forward<Args &&>(args)...);
  }

  using trivially_relocatable = is_trivially_relocatable<SetType>::type;

 private:
  SetType _set;
};
}  // namespace cct