#pragma once

#include <algorithm>
#include <compare>
#include <ostream>
#include <string_view>

#include "cct_const.hpp"
#include "cct_format.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

/// Returns the constant index (starting at 0) of given public exchange name in lower case.
/// If not found, kNbSupportedExchanges will be returned.
constexpr auto PublicExchangePos(std::string_view publicExchangeName) {
  return std::ranges::find_if(
             kSupportedExchanges,
             [publicExchangeName](const auto exchangeStr) { return exchangeStr == publicExchangeName; }) -
         std::begin(kSupportedExchanges);
}

class ExchangeName {
 public:
  static bool IsValid(std::string_view str);

  /// Constructs an ExchangeName with an invalid empty name.
  ExchangeName() noexcept = default;

  /// Constructs a ExchangeName with a unique identifier name.
  /// Two cases:
  ///  - either there is no '_', in this case, keyName will be empty
  ///  - either there is a '_', in this case 'globalExchangeName' will be parsed as '<exchangeName>_<keyName>'
  /// Important: it is ok to have '_' in the key name itself, but forbidden in the exchange name as it is the
  /// first '_' that is important.
  explicit ExchangeName(std::string_view globalExchangeName);

  ExchangeName(std::string_view exchangeName, std::string_view keyName);

  std::string_view name() const {
    const auto underscore = underscorePos();
    return {_nameWithKey.data(), underscore == string::npos ? _nameWithKey.size() : underscore};
  }

  std::string_view keyName() const {
    const auto underscore = underscorePos();
    return {_nameWithKey.begin() + (underscore == string::npos ? _nameWithKey.size() : underscore + 1U),
            _nameWithKey.end()};
  }

  auto publicExchangePos() const { return PublicExchangePos(name()); }

  bool isKeyNameDefined() const { return underscorePos() != string::npos; }

  std::string_view str() const { return _nameWithKey; }

  bool operator==(const ExchangeName &) const noexcept = default;
  std::strong_ordering operator<=>(const ExchangeName &) const noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const ExchangeName &rhs) { return os << rhs.str(); }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  static constexpr auto kMinExchangeNameLength =
      std::ranges::min_element(kSupportedExchanges, [](auto lhs, auto rhs) { return lhs.size() < rhs.size(); })
          -> size();

  string::size_type underscorePos() const { return _nameWithKey.find('_', kMinExchangeNameLength); }

  string _nameWithKey;
};

}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::ExchangeName> {
  /// format ExchangeName 'name_key':
  ///  - '{}' -> 'name_key'
  ///  - '{:e}' -> 'name'
  ///  - '{:n}' -> 'name'
  ///  - '{:k}' -> 'key'
  ///  - '{:ek}' -> 'name_key'
  bool printExchangeName = false;
  bool printKeyName = false;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    const auto end = ctx.end();
    if (it == end || *it == '}') {
      printExchangeName = true;
      printKeyName = true;
    } else {
      for (; it != end; ++it) {
        switch (*it) {
          case 'e':
            [[fallthrough]];
          case 'n':
            printExchangeName = true;
            break;
          case 'k':
            printKeyName = true;
            break;
          case '}':
            return it;
          default:
            throw format_error("invalid format");
        }
      }
    }

    return end;
  }

  template <typename FormatContext>
  auto format(const cct::ExchangeName &exchangeName, FormatContext &ctx) const -> decltype(ctx.out()) {
    if (printExchangeName) {
      ctx.out() = fmt::format_to(ctx.out(), "{}", exchangeName.name());
    }
    if (printKeyName && exchangeName.isKeyNameDefined()) {
      ctx.out() = fmt::format_to(ctx.out(), "{}{}", printExchangeName ? "_" : "", exchangeName.keyName());
    }
    return ctx.out();
  }
};
#endif