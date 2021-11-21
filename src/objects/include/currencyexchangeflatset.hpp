#pragma once

#include "cct_exception.hpp"
#include "cct_flatset.hpp"
#include "cct_type_traits.hpp"
#include "currencyexchange.hpp"

namespace cct {

/// CurrencyExchange FlatSet with possibility to query find / contains with standard CurrencyCode instead of
/// CurrencyExchange.
class CurrencyExchangeFlatSet {
 private:
  struct CompareCurrencyExchangeByStandardCode {
    bool operator()(const CurrencyExchange &lhs, const CurrencyExchange &rhs) const {
      return lhs.standardCode() < rhs.standardCode();
    }
  };
  using SetType = FlatSet<CurrencyExchange, CompareCurrencyExchangeByStandardCode>;

 public:
  using iterator = SetType::iterator;
  using const_iterator = SetType::const_iterator;
  using size_type = SetType::size_type;

  const CurrencyExchange &front() const { return _set.front(); }
  const CurrencyExchange &back() const { return _set.back(); }

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

  const_iterator find(const CurrencyExchange &v) const { return _set.find(v); }
  bool contains(const CurrencyExchange &v) const { return find(v) != end(); }

  const_iterator find(CurrencyCode standardCode) const {
    // This is possible as CurrencyExchanges are ordered by standard code
    const_iterator lbIt =
        std::lower_bound(_set.begin(), _set.end(), standardCode,
                         [](const CurrencyExchange &lhs, CurrencyCode c) { return lhs.standardCode() < c; });
    return lbIt == end() || standardCode < lbIt->standardCode() ? end() : lbIt;
  }

  const CurrencyExchange &getOrThrow(CurrencyCode standardCode) const {
    const_iterator it = find(standardCode);
    if (it == _set.end()) {
      string ex("Unknown ");
      ex.append(standardCode.str());
      throw exception(std::move(ex));
    }
    return *it;
  }

  bool contains(CurrencyCode standardCode) const { return find(standardCode) != end(); }

  std::pair<iterator, bool> insert(const CurrencyExchange &v) { return _set.insert(v); }

  std::pair<iterator, bool> insert(CurrencyExchange &&v) { return _set.insert(std::move(v)); }

  template <class... Args>
  std::pair<iterator, bool> emplace(Args &&...args) {
    return _set.emplace(std::forward<Args &&>(args)...);
  }

  using trivially_relocatable = is_trivially_relocatable<SetType>::type;

 private:
  SetType _set;
};
}  // namespace cct