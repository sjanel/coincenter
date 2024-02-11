#pragma once

#include <array>
#include <functional>
#include <ostream>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "monetaryamount.hpp"

namespace cct {
/// Represents a tradable market from a currency pair.
/// Could be a fiat / coin or a coin / coin couple (fiat / fiat couple is possible but probably not relevant).
/// Important note: BTC/ETH != ETH/BTC. Use reverse() to reverse it.
class Market {
 public:
  enum class Type : int8_t { kRegularExchangeMarket, kFiatConversionMarket };

  Market() noexcept(std::is_nothrow_default_constructible_v<CurrencyCode>) = default;

  Market(CurrencyCode first, CurrencyCode second, Type type = Type::kRegularExchangeMarket) : _assets({first, second}) {
    setType(type);
  }

  /// Create a Market from its string representation.
  /// The two currency codes must be separated by given char separator.
  explicit Market(std::string_view marketStrRep, char currencyCodeSep = '-', Type type = Type::kRegularExchangeMarket);

  bool isDefined() const { return base().isDefined() && quote().isDefined(); }

  bool isNeutral() const { return base().isNeutral() && quote().isNeutral(); }

  /// Computes the reverse market.
  /// Example: return XRP/BTC for a market BTC/XRP
  [[nodiscard]] Market reverse() const { return {_assets[1], _assets[0]}; }

  /// Get the base CurrencyCode of this Market.
  CurrencyCode base() const { return _assets[0]; }

  /// Get the quote CurrencyCode of this Market.
  CurrencyCode quote() const { return _assets[1]; }

  /// Given 'c' a currency traded in this Market, return the other currency it is paired with.
  /// If 'c' is not traded by this market, return the second currency.
  [[nodiscard]] CurrencyCode opposite(CurrencyCode cur) const { return _assets[1] == cur ? _assets[0] : _assets[1]; }

  /// Tells whether this market trades given monetary amount based on its currency.
  bool canTrade(MonetaryAmount ma) const { return canTrade(ma.currencyCode()); }

  /// Tells whether this market trades given currency code.
  bool canTrade(CurrencyCode cur) const { return std::ranges::find(_assets, cur) != _assets.end(); }

  constexpr auto operator<=>(const Market&) const noexcept = default;

  string str() const { return assetsPairStrUpper('-'); }

  Type type() const { return static_cast<Type>(_assets[0].getAdditionalBits()); }

  friend std::ostream& operator<<(std::ostream& os, const Market& mk);

  /// Returns a string representing this Market in lower case
  string assetsPairStrLower(char sep = 0) const { return assetsPairStr(sep, true); }

  /// Returns a string representing this Market in upper case
  string assetsPairStrUpper(char sep = 0) const { return assetsPairStr(sep, false); }

 private:
  string assetsPairStr(char sep, bool lowerCase) const;

  void setType(Type type) { _assets[0].uncheckedSetAdditionalBits(static_cast<int8_t>(type)); }

  std::array<CurrencyCode, 2> _assets;
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::Market> {
  constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::Market& mk, FormatContext& ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}-{}", mk.base(), mk.quote());
  }
};
#endif

namespace std {
template <>
struct hash<cct::Market> {
  auto operator()(const cct::Market& mk) const {
    return cct::HashCombine(hash<cct::CurrencyCode>()(mk.base()), hash<cct::CurrencyCode>()(mk.quote()));
  }
};
}  // namespace std
