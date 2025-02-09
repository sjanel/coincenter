#pragma once

#include <compare>
#include <limits>
#include <ostream>
#include <string_view>

#include "cct_format.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"
#include "enum-string.hpp"
#include "exchange-name-enum.hpp"

namespace cct {

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

  explicit ExchangeName(ExchangeNameEnum exchangeNameEnum, std::string_view keyName = {});

  std::string_view name() const { return EnumToString(_exchangeNameEnum); }

  std::string_view keyName() const {
    return {_nameWithKey.begin() + (_begKeyNamePos == kUndefinedKeyNamePos ? _nameWithKey.size() : _begKeyNamePos),
            _nameWithKey.end()};
  }

  std::size_t publicExchangePos() const { return static_cast<std::size_t>(_exchangeNameEnum); }

  ExchangeNameEnum exchangeNameEnum() const { return _exchangeNameEnum; }

  bool isKeyNameDefined() const { return _begKeyNamePos != kUndefinedKeyNamePos; }

  std::string_view str() const { return _nameWithKey; }

  bool operator==(const ExchangeName &) const noexcept = default;
  std::strong_ordering operator<=>(const ExchangeName &) const noexcept = default;

  friend std::ostream &operator<<(std::ostream &os, const ExchangeName &en) { return os << en.str(); }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  static constexpr uint8_t kUndefinedKeyNamePos = std::numeric_limits<uint8_t>::max();

  ExchangeNameEnum _exchangeNameEnum;
  uint8_t _begKeyNamePos;
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