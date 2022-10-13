#pragma once

#include <array>
#include <ostream>
#include <utility>

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

  Market() noexcept(std::is_nothrow_default_constructible_v<CurrencyCode>) = default;

  Market(CurrencyCode first, CurrencyCode second) : _assets({first, second}) {}

  /// Create a Market from its string representation.
  /// The two currency codes must be separated by given char separator.
  Market(std::string_view marketStrRep, char currencyCodeSep);

  bool isDefined() const { return !base().isNeutral() && !quote().isNeutral(); }

  bool isNeutral() const { return base().isNeutral() && quote().isNeutral(); }

  /// Computes the reverse market.
  /// Example: return XRP/BTC for a market BTC/XRP
  Market reverse() const { return Market(_assets[1], _assets[0]); }

  /// Get the base CurrencyCode of this Market.
  CurrencyCode base() const { return _assets[0]; }

  /// Get the quote CurrencyCode of this Market.
  CurrencyCode quote() const { return _assets[1]; }

  /// Given 'c' a currency traded in this Market, return the other currency it is paired with.
  /// If 'c' is not traded by this market, return the second currency.
  CurrencyCode opposite(CurrencyCode c) const { return _assets[1] == c ? _assets[0] : _assets[1]; }

  bool canTrade(MonetaryAmount a) const { return canTrade(a.currencyCode()); }
  bool canTrade(CurrencyCode c) const { return base() == c || quote() == c; }

  auto operator<=>(const Market&) const = default;

  string str() const { return assetsPairStrUpper('-'); }

  friend std::ostream& operator<<(std::ostream& os, const Market& m);

  /// Returns a string representing this Market in lower case
  string assetsPairStrLower(char sep = 0) const { return assetsPairStr(sep, true); }

  /// Returns a string representing this Market in upper case
  string assetsPairStrUpper(char sep = 0) const { return assetsPairStr(sep, false); }

 private:
  string assetsPairStr(char sep, bool lowerCase) const;

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