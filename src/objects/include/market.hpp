#pragma once

#include "cct_string.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
/// Represents a tradable market from a currency pair.
/// Could be a fiat / coin or a coin / coin couple (fiat / fiat couple is possible but probably not relevant).
/// Important note: BTC/ETH != ETH/BTC. Use reverse() to reverse it.
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

  auto operator<=>(const Market&) const = default;

  string str() const { return assetsPairStr('-'); }
  string assetsPairStr(char sep = 0, bool lowerCase = false) const;

  friend std::ostream& operator<<(std::ostream& os, const Market& m);

 private:
  TradableAssets _assets;
};
}  // namespace cct

namespace std {
template <>
struct hash<cct::Market> {
  size_t operator()(const cct::Market& m) const {
    return cct::HashCombine(hash<cct::CurrencyCode>()(m.base()), hash<cct::CurrencyCode>()(m.quote()));
  }
};
}  // namespace std