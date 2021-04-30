#pragma once

#include <string>

#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
/// Represents a tradable market from a currency pair.
/// Could be a fiat / coin or a coin / coin couple (fiat / fiat couple is possible but probably not relevant).
/// Important note: BTC/ETH != ETH/BTC. Use unary minus operator to reverse it.
class Market {
 public:
  using TradableAssets = std::array<CurrencyCode, 2>;

  explicit Market(std::pair<CurrencyCode, CurrencyCode> curPair) : Market(curPair.first, curPair.second) {}

  Market() = default;

  Market(CurrencyCode first, CurrencyCode second) : _assets({first, second}) {}

  /// Create a Market from its string representation.
  /// The two currency codes must be separated by given char separator.
  Market(std::string_view marketStrRep, char currencyCodeSep);

  /// Computes the reverse market.
  /// Example: return XRP/BTC for a market BTC/XRP
  Market reverse() const { return Market(_assets.back(), _assets.front()); }

  CurrencyCode base() const { return _assets.front(); }
  CurrencyCode quote() const { return _assets.back(); }

  bool canTrade(MonetaryAmount a) const { return canTrade(a.currencyCode()); }
  bool canTrade(CurrencyCode c) const { return base() == c || quote() == c; }

  bool operator<(Market o) const {
    return std::lexicographical_compare(_assets.begin(), _assets.end(), o._assets.begin(), o._assets.end());
  }
  bool operator<=(Market o) const { return !(o < *this); }
  bool operator>(Market o) const { return o < *this; }
  bool operator>=(Market o) const { return !(*this < o); }

  bool operator==(const Market& o) const { return std::equal(_assets.begin(), _assets.end(), o._assets.begin()); }
  bool operator!=(const Market& o) const { return !(*this == o); }

  std::string str() const { return assetsPairStr('-'); }
  std::string assetsPairStr(char sep = 0) const;

  friend std::ostream& operator<<(std::ostream& os, const Market& m);

 private:
  TradableAssets _assets;
};
}  // namespace cct

namespace std {
template <>
struct hash<cct::Market> {
  std::size_t operator()(const cct::Market& m) const {
    return cct::HashCombine(std::hash<cct::CurrencyCode>{}(m.base()), std::hash<cct::CurrencyCode>{}(m.quote()));
  }
};
}  // namespace std