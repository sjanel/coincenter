#pragma once

#include <algorithm>
#include <ostream>
#include <span>
#include <string_view>

#include "cct_const.hpp"
#include "cct_format.hpp"
#include "cct_smallvector.hpp"
#include "cct_string.hpp"
#include "cct_type_traits.hpp"

namespace cct {

class ExchangeName {
 public:
  ExchangeName() noexcept(std::is_nothrow_default_constructible_v<string>) = default;

  /// Constructs a ExchangeName with a unique identifier name.
  /// Two cases:
  ///  - either there is no '_', in this case, keyName will be empty
  ///  - either there is a '_', in this case 'globalExchangeName' will be parsed as '<exchangeName>_<keyName>'
  /// Important: it is ok to have '_' in the key name itself, but forbidden in the exchange name as it is the
  /// first '_' that is important.
  explicit ExchangeName(std::string_view globalExchangeName);

  ExchangeName(std::string_view exchangeName, std::string_view keyName);

  std::string_view name() const {
    std::size_t underscore = underscorePos();
    return std::string_view(_nameWithKey.data(), underscore == string::npos ? _nameWithKey.size() : underscore);
  }

  std::string_view keyName() const {
    std::size_t underscore = underscorePos();
    return std::string_view(_nameWithKey.begin() + (underscore == string::npos ? _nameWithKey.size() : underscore + 1U),
                            _nameWithKey.end());
  }

  bool isKeyNameDefined() const { return underscorePos() != string::npos; }

  std::string_view str() const { return _nameWithKey; }

  bool operator==(const ExchangeName &) const = default;

  friend std::ostream &operator<<(std::ostream &os, const ExchangeName &v) {
    os << v.str();
    return os;
  }

  using trivially_relocatable = is_trivially_relocatable<string>::type;

 private:
  std::size_t underscorePos() const { return _nameWithKey.find('_', kMinExchangeNameLength); }

  static constexpr std::size_t kMinExchangeNameLength =
      std::ranges::min_element(kSupportedExchanges, [](std::string_view lhs, std::string_view rhs) {
        return lhs.size() < rhs.size();
      })->size();

  string _nameWithKey;
};

using ExchangeNameSpan = std::span<const ExchangeName>;
using ExchangeNames = SmallVector<ExchangeName, kTypicalNbPrivateAccounts>;

inline std::string_view ToString(std::string_view exchangeName) { return exchangeName; }
inline std::string_view ToString(const ExchangeName &exchangeName) { return exchangeName.str(); }

template <class ExchangeNames>
inline string ConstructAccumulatedExchangeNames(const ExchangeNames &exchangeNames) {
  string exchangesStr(exchangeNames.empty() ? "all" : "");
  for (const auto &exchangeName : exchangeNames) {
    if (!exchangesStr.empty()) {
      exchangesStr.push_back(',');
    }
    exchangesStr.append(ToString(exchangeName));
  }
  return exchangesStr;
}
}  // namespace cct

#ifndef CCT_DISABLE_SPDLOG
template <>
struct fmt::formatter<cct::ExchangeName> {
  /// format ExchangeName 'name_key':
  ///  - '{}' -> 'name'
  ///  - '{:e}' -> 'name'
  ///  - '{:n}' -> 'name'
  ///  - '{:k}' -> 'key'
  ///  - '{:ek}' -> 'name_key'
  bool printExchangeName = false;
  bool printKeyName = false;

  constexpr auto parse(format_parse_context &ctx) -> decltype(ctx.begin()) {
    auto it = ctx.begin();
    for (auto end = ctx.end(); it != end; ++it) {
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
    return it;
  }

  template <typename FormatContext>
  auto format(const cct::ExchangeName &e, FormatContext &ctx) const -> decltype(ctx.out()) {
    return fmt::format_to(ctx.out(), "{}", printKeyName ? (printExchangeName ? e.str() : e.keyName()) : e.name());
  }
};
#endif