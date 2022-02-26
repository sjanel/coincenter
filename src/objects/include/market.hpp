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

  explicit Market(std::pair<CurrencyCode, CurrencyCode> curPair) : Market(curPair.first, curPair.second) {}

  Market() noexcept(std::is_nothrow_default_constructible_v<CurrencyCode>) = default;

  Market(CurrencyCode first, CurrencyCode second) : _assets({first, second}) {}

  /// Create a Market from its string representation.
  /// The two currency codes must be separated by given char separator.
  Market(std::string_view marketStrRep, char currencyCodeSep);

  bool isDefined() const { return !base().isNeutral() && !quote().isNeutral(); }

  bool isNeutral() const { return base().isNeutral() && quote().isNeutral(); }

  /// Computes the reverse market.
  /// Example: return XRP/BTC for a market BTC/XRP
  Market reverse() const { return Market(_assets.back(), _assets.front()); }

  /// Get the base CurrencyCode of this Market.
  /// Beware: do not use this method to get a string view of the Currency, prefer 'baseStr' instead as it returns a
  /// CurrencyCode by copy.
  CurrencyCode base() const { return _assets.front(); }

  /// Get the quote CurrencyCode of this Market.
  /// Beware: do not use this method to get a string view of the Currency, prefer 'quoteStr' instead as it returns a
  /// CurrencyCode by copy.
  CurrencyCode quote() const { return _assets.back(); }

  /// Returns a string_view on the base currency of this market.
  std::string_view baseStr() const { return _assets.front().str(); }

  /// Returns a string_view on the quote currency of this market.
  std::string_view quoteStr() const { return _assets.back().str(); }

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