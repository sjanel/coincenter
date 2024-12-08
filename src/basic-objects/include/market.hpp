#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <ostream>

#include "cct_format.hpp"
#include "cct_json-serialization.hpp"
#include "cct_string.hpp"
#include "currencycode.hpp"
#include "generic-object-json.hpp"

namespace cct {

struct MarketBase {
  struct StringOutputConfig {
    char currencyCodeSep = '-';
    bool lowerCase = false;
  };
};

/// Represents a tradable market from a currency pair.
/// Could be a fiat / coin or a coin / coin couple (fiat / fiat couple is possible but probably not relevant).
/// Important note: BTC/ETH != ETH/BTC. Use reverse() to reverse it.
class Market {
 public:
  enum class Type : int8_t { kRegularExchangeMarket, kFiatConversionMarket };

  static constexpr auto kMaxLen = CurrencyCode::kMaxLen * 2 + 2U;  // 1 for sep, 1 for '*' if fiat conversion

  constexpr Market() noexcept(std::is_nothrow_default_constructible_v<CurrencyCode>) = default;

  constexpr Market(CurrencyCode first, CurrencyCode second, Type type = Type::kRegularExchangeMarket)
      : _assets({first, second}) {
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

  /// Get the string length representation of this Market.
  uint32_t strLen(bool withSep = true) const noexcept {
    return base().size() + quote().size() + static_cast<uint32_t>(type() == Type::kFiatConversionMarket) +
           static_cast<uint32_t>(withSep);
  }

  /// Given 'c' a currency traded in this Market, return the other currency it is paired with.
  /// If 'c' is not traded by this market, return the second currency.
  [[nodiscard]] CurrencyCode opposite(CurrencyCode cur) const { return _assets[1] == cur ? _assets[0] : _assets[1]; }

  /// Tells whether this market trades given currency code.
  bool canTrade(CurrencyCode cur) const { return cur == base() || cur == quote(); }

  constexpr auto operator<=>(const Market &) const noexcept = default;

  string str() const { return assetsPairStrUpper('-'); }

  Type type() const noexcept { return static_cast<Type>(_assets[0].getAdditionalBits()); }

  friend std::ostream &operator<<(std::ostream &os, const Market &mk);

  /// Returns a string representing this Market in lower case
  string assetsPairStrLower(char sep = '\0') const {
    return assetsPairStr(MarketBase::StringOutputConfig{.currencyCodeSep = sep, .lowerCase = true});
  }

  /// Returns a string representing this Market in upper case
  string assetsPairStrUpper(char sep = '\0') const {
    return assetsPairStr(MarketBase::StringOutputConfig{.currencyCodeSep = sep, .lowerCase = false});
  }

  /// Append market string representation to given string.
  template <class StringT>
  void appendStrTo(StringT &str,
                   MarketBase::StringOutputConfig stringOutputConfig = MarketBase::StringOutputConfig{}) const {
    const auto len = strLen(stringOutputConfig.currencyCodeSep != '\0');
    str.append(len, '\0');
    appendTo(str.end() - len, stringOutputConfig);
  }

  /// Append currency string representation to given output iterator
  template <class OutputIt>
  constexpr OutputIt appendTo(
      OutputIt it, MarketBase::StringOutputConfig stringOutputConfig = MarketBase::StringOutputConfig{}) const {
    if (type() == Type::kFiatConversionMarket) {
      *it = '*';
      ++it;
    }
    auto begIt = it;
    it = base().appendTo(it);
    if (stringOutputConfig.currencyCodeSep != '\0') {
      *it = stringOutputConfig.currencyCodeSep;
      ++it;
    }
    it = quote().appendTo(it);
    if (stringOutputConfig.lowerCase) {
      std::transform(begIt, it, begIt, tolower);
    }
    return it;
  }

 private:
  string assetsPairStr(MarketBase::StringOutputConfig stringOutputConfig) const {
    string ret(strLen(stringOutputConfig.currencyCodeSep != '\0'), '\0');
    appendTo(ret.begin(), stringOutputConfig);
    return ret;
  }

  constexpr void setType(Type type) { _assets[0].uncheckedSetAdditionalBits(static_cast<int8_t>(type)); }

  std::array<CurrencyCode, 2> _assets;
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<::cct::Market> {
  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    const auto end = ctx.end();
    if (it != end && *it != '}') {
      throw format_error("invalid format");
    }
    return it;
  }

  template <typename FormatContext>
  auto format(const ::cct::Market &mk, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}-{}", mk.base(), mk.quote());
  }
};
#endif

namespace std {
template <>
struct hash<::cct::Market> {
  auto operator()(const ::cct::Market &mk) const {
    return ::cct::HashCombine(hash<::cct::CurrencyCode>()(mk.base()), hash<::cct::CurrencyCode>()(mk.quote()));
  }
};
}  // namespace std

namespace glz::detail {
template <>
struct from<JSON, ::cct::Market> {
  template <auto Opts, class It, class End>
  static void op(auto &&value, is_context auto &&, It &&it, End &&end) noexcept {
    // used as a value. As a key, the first quote will not be present.
    auto endIt = std::find(*it == '"' ? ++it : it, end, '"');
    value = ::cct::Market(std::string_view(it, endIt));
    it = ++endIt;
  }
};

template <>
struct to<JSON, ::cct::Market> {
  template <auto Opts, is_context Ctx, class B, class IX>
  static void op(auto &&value, Ctx &&, B &&b, IX &&ix) {
    ::cct::details::ToStrLikeJson<Opts>(value, b, ix);
  }
};
}  // namespace glz::detail
